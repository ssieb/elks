/* Initialize Ethernet driver */

#include <linuxmt/config.h>
#include <linuxmt/init.h>

void eth_init(void)
{
#ifdef CONFIG_ETH_NE2K
	ne2k_drv_init();
#endif
#ifdef CONFIG_ETH_WD
	wd_drv_init();
#endif
#ifdef CONFIG_ETH_EL3
	el3_drv_init();
#endif
}
