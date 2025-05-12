#include <linux/atomic.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/of_device.h>

#define DEVICE_NAME "zynq_irq" // Driver name
#define MAX_IRQS 2               // Support 2 interrupt channels
#define LOG(fmt, ...) printk(KERN_INFO "[ZYNQ_IRQ] " fmt "\n", ##__VA_ARGS__)

/* Per-interrupt private data structure */
struct irq_priv {
  int irq_num;                  // Linux IRQ number
  atomic_t data_ready;          // Atomic flag for data status
  atomic_t is_open;             // Atomic flag for device open status
  struct fasync_struct *fasync; // Async notification queue
  char name[16];                // IRQ identifier
};

static int major;             // Major device number
static struct class *cls;     // Device class
static struct irq_priv *irqs; // Array of IRQ data structures

/* File operations: open */
static int irq_open(struct inode *inode, struct file *file) {
  int minor = iminor(inode);

  if (minor >= MAX_IRQS) {
    LOG("Invalid minor number %d", minor);
    return -ENODEV;
  }

  atomic_set(&irqs[minor].is_open, 1);
  LOG("Opened device %s", irqs[minor].name);
  return 0;
}

/* File operations: release */
static int irq_release(struct inode *inode, struct file *file) {
  int minor = iminor(inode);
  atomic_set(&irqs[minor].is_open, 0);
  LOG("Closed device %s", irqs[minor].name);
  return 0;
}

/* File operations: read */
static ssize_t irq_read(struct file *file, char __user *buf, size_t count,
                        loff_t *ppos) {
  int minor = iminor(file->f_inode);
  int val = atomic_read(&irqs[minor].data_ready);

  if (copy_to_user(buf, &val, sizeof(val))) {
    LOG("Failed to copy data to user for IRQ %d", irqs[minor].irq_num);
    return -EFAULT;
  }

  atomic_set(&irqs[minor].data_ready, 0);
  LOG("Read %d from IRQ %d", val, irqs[minor].irq_num);
  return sizeof(val);
}

/* File operations: fasync - register/unregister async notification */
static int irq_fasync(int fd, struct file *filp, int on) {
  int minor = iminor(filp->f_inode);
  int ret = fasync_helper(fd, filp, on, &irqs[minor].fasync);

  if (ret >= 0)
    LOG("%s async notification for IRQ %d", on ? "Enabled" : "Disabled",
        irqs[minor].irq_num);
  else
    LOG("Failed to %s async notification for IRQ %d", on ? "enable" : "disable",
        irqs[minor].irq_num);

  return ret;
}

/* File operations structure */
static const struct file_operations irq_fops = {
    .owner = THIS_MODULE,
    .open = irq_open,
    .release = irq_release,
    .read = irq_read,
    .fasync = irq_fasync,
};

/* Interrupt handler - called when PL triggers interrupt */
static irqreturn_t irq_handler(int irq, void *dev_id) {
  struct irq_priv *priv = dev_id;

  // Mark data as ready
  atomic_set(&priv->data_ready, 1);
  LOG("IRQ %d triggered (%s)", irq, priv->name);

  // Send signal if device is open and listeners exist
  if (atomic_read(&priv->is_open) && priv->fasync) {
    kill_fasync(&priv->fasync, SIGIO, POLL_IN);
    LOG("Sent SIGIO for IRQ %d", irq);
  }

  return IRQ_HANDLED;
}

/* Probe function - called when device is matched */
static int irq_probe(struct platform_device *pdev) {
  int ret, i;

  // Allocate IRQ private data
  irqs =
      devm_kzalloc(&pdev->dev, MAX_IRQS * sizeof(struct irq_priv), GFP_KERNEL);
  if (!irqs) {
    LOG("Failed to allocate memory for IRQ data");
    return -ENOMEM;
  }

  // Register character device
  major = register_chrdev(0, DEVICE_NAME, &irq_fops);
  if (major < 0) {
    LOG("Failed to register char device");
    return major;
  }

  // Create device class
  cls = class_create(THIS_MODULE, DEVICE_NAME);
  if (IS_ERR(cls)) {
    ret = PTR_ERR(cls);
    LOG("Failed to create class");
    goto fail_chrdev;
  }

  // Initialize each IRQ channel
  for (i = 0; i < MAX_IRQS; i++) {
    // Get IRQ number from device tree
    irqs[i].irq_num = platform_get_irq(pdev, i);
    if (irqs[i].irq_num < 0) {
      LOG("Failed to get IRQ %d from device tree", i);
      ret = irqs[i].irq_num;
      goto fail_irq;
    }

    // Initialize atomic flags
    atomic_set(&irqs[i].data_ready, 0);
    atomic_set(&irqs[i].is_open, 0);

    // Set IRQ name
    snprintf(irqs[i].name, sizeof(irqs[i].name), "zynq_irq_%d", i);

    // Register interrupt handler
    ret = devm_request_threaded_irq(
        &pdev->dev, irqs[i].irq_num, NULL, irq_handler,
        IRQF_TRIGGER_RISING | IRQF_ONESHOT, irqs[i].name, &irqs[i]);
    if (ret) {
      LOG("Failed to request IRQ %d (err=%d)", irqs[i].irq_num, ret);
      goto fail_irq;
    }

    // Create device node
    device_create(cls, NULL, MKDEV(major, i), NULL, "zynq_irq_%d", i);
    LOG("Initialized IRQ %d (%s)", irqs[i].irq_num, irqs[i].name);
  }

  LOG("Driver probed successfully");
  return 0;

fail_irq:
  while (--i >= 0) {
    device_destroy(cls, MKDEV(major, i));
  }
  class_destroy(cls);
fail_chrdev:
  unregister_chrdev(major, DEVICE_NAME);
  return ret;
}

/* Remove function - called when device is unbound */
static int irq_remove(struct platform_device *pdev) {
  int i;
  for (i = 0; i < MAX_IRQS; i++) {
    device_destroy(cls, MKDEV(major, i));
    LOG("Removed device for IRQ %d", irqs[i].irq_num);
  }
  class_destroy(cls);
  unregister_chrdev(major, DEVICE_NAME);
  LOG("Driver unloaded");
  return 0;
}

/* Device tree match table */
static const struct of_device_id irq_of_match[] = {
    {.compatible = "fei,zynq-irq"}, // Must match device tree
    {}};
MODULE_DEVICE_TABLE(of, irq_of_match);

/* Platform driver structure */
static struct platform_driver irq_driver = {
    .probe = irq_probe,
    .remove = irq_remove,
    .driver =
        {
            .name = "zynq_irq",
            .of_match_table = irq_of_match,
        },
};
module_platform_driver(irq_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("fei");
MODULE_DESCRIPTION("Zynq PL Interrupt Driver with Async Notification");
