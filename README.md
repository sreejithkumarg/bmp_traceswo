# README for bmp_traceswo:

_This has been modified to work properly for my STM32F107 board. I make no guarantees that it works for other boards - the original version just didn't work._

A simple application which uses libusb-1.0 to open the TRACESWO USB endpoint
on the Black Magic Probe and dump the incoming data. Note that the TRACESWO
port doesn't advertise itself as a modem, this is because it provides raw
packets rather than a character stream. The program decodes the packets and
dumps the resulting character stream to stdout. Invalid packets are common
and occur during bootup because of the Black Magic Probe losing sync with the
Manchester encoded stream (probably because the TRACESWO pin briefly gets
disabled and goes tri-state until the program gets running again). When an
invalid packet is received it is dumped to stderr without interrupting the
program flow. Similarly, if the Black Magic Probe is unplugged or not found,
the program attempts to recover by trying to re-enumerate and re-open the
USB port once per second. Thus the program can be left running in background.

Please be warned that using ITM_SendChar() will block your application, it
is not really supposed to block (documentation states that ITM logging can be
left in your released product, and additionally the TRACESWO pin is not flow-
controller since the ARM has no way to know whether the output is being read),
but it seems that TRACESWO is a special case because the ARM knows that the
TRACESWO pin is not active except when the device is in SWD mode, so it tells
the client that nothing is reading the ITM_SendChar() output thus blocking it.

Put some code like this in your application, this is for STM32 family:

/* USER CODE BEGIN PFP */
/* Private function prototypes -----------------------------------------------*/

int _write(int file, char *ptr, int len);
/* USER CODE END PFP */

/* USER CODE BEGIN 0 */
#define SWO_FREQ 115200
#define HCLK_FREQ 216000000

/* Pelican TPIU
 * cut down TPIU implemented in STM32F7 series
 * see en.DM00224583.pdf page 1882
 */
#define TPIU_CURRENT_PORT_SIZE *((volatile unsigned *)(0xE0040004))
#define TPIU_ASYNC_CLOCK_PRESCALER *((volatile unsigned *)(0xE0040010))
#define TPIU_SELECTED_PIN_PROTOCOL *((volatile unsigned *)(0xE00400F0))
#define TPIU_FORMATTER_AND_FLUSH_CONTROL *((volatile unsigned *)(0xE0040304))

int _write(int file, char *ptr, int len) {
  for (int i = 0; i < len; ++i)
    ITM_SendChar(*ptr++);
  return len;
}
/* USER CODE END 0 */

int main(void)
{

  /* USER CODE BEGIN 1 */
  CoreDebug->DEMCR = CoreDebug_DEMCR_TRCENA_Msk;
  DBGMCU->CR = DBGMCU_CR_DBG_SLEEP_Msk | DBGMCU_CR_DBG_STOP_Msk | DBGMCU_CR_DBG_STANDBY_Msk | DBGMCU_CR_TRACE_IOEN_Msk;

  TPIU_CURRENT_PORT_SIZE = 1; /* port size = 1 bit */
  TPIU_SELECTED_PIN_PROTOCOL = 1; /* trace port protocol = Manchester */
  TPIU_ASYNC_CLOCK_PRESCALER = (HCLK_FREQ / SWO_FREQ) - 1;
  TPIU_FORMATTER_AND_FLUSH_CONTROL = 0x100; /* turn off formatter (0x02 bit) */

  ITM->LAR = 0xC5ACCE55;
  ITM->TCR = ITM_TCR_TraceBusID_Msk | ITM_TCR_SWOENA_Msk | ITM_TCR_SYNCENA_Msk | ITM_TCR_ITMENA_Msk;
  ITM->TPR = ITM_TPR_PRIVMASK_Msk; /* all ports accessible unprivileged */
  ITM->TER = 1; /* enable stimulus channel 0, used with ITM_SendChar() */

  /* this apparently turns off sync packets, see SYNCTAP in DDI0403D pdf: */
  DWT->CTRL = 0x400003FE;
  /* USER CODE END 1 */

  ... remainder of automatically generated main() function
}

If using STM32CubeMX put it in the USER CODE areas as demonstrated above.

Remember to use the "monitor traceswo" command in gdb, I use this .gdbinit:
target extended-remote /dev/ttyBmpGdb
monitor swdp_scan
attach 1
monitor traceswo
set mem inaccessible-by-default off

I also have this /etc/udev/rules.d/99-blackmagic.rules to create symlinks:
SUBSYSTEM=="tty", ATTRS{interface}=="Black Magic GDB Server", SYMLINK+="ttyBmpGdb"
SUBSYSTEM=="tty", ATTRS{interface}=="Black Magic UART Port", SYMLINK+="ttyBmpTarg"

Thanks a lot to the following site maintained by Kornelius Rohmeyer:
http://algorithm-forge.com/techblog/2009/12/using-libusb-to-write-a-linux-usb-driver-for-the-arexx-tl-500-part-i/
