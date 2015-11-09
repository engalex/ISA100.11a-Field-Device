ISA100.11a - Field-Device Communication Stack
=============================

For now, the source code is compatible with Freescale MC13224-based platforms, and in order to compile it, the following are needed:

- Freescale BeeKit
http://www.freescale.com/webapp/sps/site/prod_summary.jsp?code=BEEKIT_WIRELESS_CONNECTIVITY_TOOLKIT&fpsp=1&tab=Design_Tools_Tab

- Freescale libraries (v1.1.4):
    libs/LLC.a
    libs/MC1322x.a

- Freescale library interfaces:
    LibInterface/Crm.h
    LibInterface/CrmRegs.h
    LibInterface/EmbeddedTypes.h
    LibInterface/gpio.h
    LibInterface/ITC_Interface.h
    LibInterface/NVM.h
    LibInterface/Platform.h
    LibInterface/SPI_Interface.h
    LibInterface/Ssi_Regs.h
    LibInterface/Timer.h
