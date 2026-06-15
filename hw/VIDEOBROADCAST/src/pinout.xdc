set_property PACKAGE_PIN K17 [get_ports CLK_I] 
set_property IOSTANDARD LVCMOS33 [get_ports CLK_I]

##Switches
set_property -dict { PACKAGE_PIN G15   IOSTANDARD LVCMOS33 } [get_ports { SW_COLOR_MODE }]; #IO_L19N_T3_VREF_35 Sch=sw[0]

##Buttons
set_property -dict { PACKAGE_PIN K18   IOSTANDARD LVCMOS33 } [get_ports { RST_I }]; #IO_L12N_T1_MRCC_35 Sch=btn[0]
set_property -dict { PACKAGE_PIN Y16   IOSTANDARD LVCMOS33 } [get_ports { RESETCAMBUTTON }]; #IO_L7P_T1_34 Sch=btn[3]

set_property PACKAGE_PIN H16 [get_ports HDMI_CLK_P] 
set_property PACKAGE_PIN D19 [get_ports HDMI_D0_P] 
set_property PACKAGE_PIN C20 [get_ports HDMI_D1_P] 
set_property PACKAGE_PIN B19 [get_ports HDMI_D2_P] 
set_property IOSTANDARD TMDS_33 [get_ports HDMI_CLK_*] 
set_property IOSTANDARD TMDS_33 [get_ports HDMI_D*] 
#set_property PACKAGE_PIN F17 [get_ports {HDMI_OEN[0]}] 
#set_property IOSTANDARD LVCMOS33 [get_ports {HDMI_OEN[0]}]

set_property PACKAGE_PIN M14 [get_ports {LED_O[0]}] 
set_property PACKAGE_PIN M15 [get_ports {LED_O[1]}] 
set_property PACKAGE_PIN G14 [get_ports {LED_O[2]}] 
set_property PACKAGE_PIN D18 [get_ports {LED_O[3]}] 
set_property IOSTANDARD LVCMOS33 [get_ports {LED_O[*]}]

# Camera OV7670

# Top JC
set_property PACKAGE_PIN V15 [get_ports pwdn]
set_property PACKAGE_PIN W15 [get_ports {datain[0]}]
set_property PACKAGE_PIN T11 [get_ports {datain[2]}]
set_property PACKAGE_PIN T10 [get_ports {datain[4]}]


# Bottom JC
set_property PACKAGE_PIN W14 [get_ports RESETCAM]
set_property PACKAGE_PIN Y14 [get_ports {datain[1]}]
set_property PACKAGE_PIN T12 [get_ports {datain[3]}]
set_property PACKAGE_PIN U12 [get_ports {datain[5]}]


# Top JD
set_property PACKAGE_PIN T14 [get_ports iic_0_scl_io]
set_property PACKAGE_PIN T15 [get_ports xclk]
set_property PACKAGE_PIN P14 [get_ports {datain[7]}]
set_property PACKAGE_PIN R14 [get_ports href]

# Bottom JD
set_property PACKAGE_PIN U14 [get_ports pclk]
set_property PACKAGE_PIN U15 [get_ports iic_0_sda_io]
set_property PACKAGE_PIN V17 [get_ports vsync]
set_property PACKAGE_PIN V18 [get_ports {datain[6]}]

# Voltage levels
set_property IOSTANDARD LVCMOS33 [get_ports pclk]
set_property IOSTANDARD LVCMOS33 [get_ports vsync]
set_property IOSTANDARD LVCMOS33 [get_ports RESETCAM]
set_property IOSTANDARD LVCMOS33 [get_ports pwdn]
set_property IOSTANDARD LVCMOS33 [get_ports href]
set_property IOSTANDARD LVCMOS33 [get_ports xclk]
set_property IOSTANDARD LVCMOS33 [get_ports {datain[*]}]

# JD4 y JD10
set_property IOSTANDARD LVCMOS33 [get_ports iic_0_scl_io]
set_property IOSTANDARD LVCMOS33 [get_ports iic_0_sda_io]
set_property PULLUP true [get_ports iic_0_sda_io]
set_property PULLUP true [get_ports iic_0_scl_io]
set_property SLEW SLOW [get_ports iic_0_sda_io]
set_property SLEW SLOW [get_ports iic_0_scl_io]

