set_property PACKAGE_PIN AP14 [get_ports {gpio_rtl_0_tri_io[0]}]
set_property IOSTANDARD LVCMOS18 [get_ports {gpio_rtl_0_tri_io[0]}]
set_property C_CLK_INPUT_FREQ_HZ 300000000 [get_debug_cores dbg_hub]
set_property C_ENABLE_CLK_DIVIDER false [get_debug_cores dbg_hub]
set_property C_USER_SCAN_CHAIN 1 [get_debug_cores dbg_hub]
connect_debug_port dbg_hub/clk [get_nets clk]

