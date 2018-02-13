/* Author: Nick Downing downing.nick@gmail.com
 * License: GPLv3, see LICENSE in repository root
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <libusb-1.0/libusb.h>

#include <queue>

uint16_t VENDOR = 0x1d50;
uint16_t PRODUCT = 0x6018;

int main() {
	int deviceIdx = 0;

	unsigned char ENDPOINT_UP = 0x85;
	int count;
	unsigned char dataUp[128];

	// data read queue
	std::queue<unsigned char> usbReadQueue;

	// init libusb
	int res = libusb_init(0);
	if(res < 0) {
		fprintf(stderr, "libusb_init: %s\n", libusb_strerror((enum libusb_error) res));
		exit(1);
	}

	while (1) {
		// get list of all devices
		libusb_device **devs;
		ssize_t cnt = libusb_get_device_list(0, &devs);
		if(cnt < 0) {
			fprintf(stderr, "libusb_get_device_list: %s\n", libusb_strerror((enum libusb_error) cnt));
			exit(1);
		}

		// find the BMP
		printf("Trying to find Black Magic probe.\n");
		for(int i = 0; devs[i]; ++i) {
			struct libusb_device_descriptor desc;
			int res = libusb_get_device_descriptor(devs[i], &desc);

			if (res < 0) {
				fprintf(stderr, "libusb_get_device_descriptor: %s\n", libusb_strerror((enum libusb_error) res));
				exit(1);
			}

			// check if the vendor and product id match
			printf("%04x:%04x (bus %d, device %d)\n", desc.idVendor, desc.idProduct, libusb_get_bus_number(devs[i]), libusb_get_device_address(devs[i]));

			if(desc.idVendor == VENDOR && desc.idProduct == PRODUCT) {
				// we found it!
				deviceIdx = i;
				goto found;
			}
		}

		// couldn't find it :(
		fprintf(stderr, "Black Magic probe not found.\n");
		goto lost_device;

found: ;
		// open the handle to talk to the device
		libusb_device_handle *handle;
		res = libusb_open(devs[deviceIdx], &handle);
		libusb_free_device_list(devs, 1);

		if(res < 0) {
			fprintf(stderr, "libusb_open: %s\n", libusb_strerror((enum libusb_error) res));

			// if the device fucked off, search again
			if(res == LIBUSB_ERROR_NO_DEVICE) {
				goto lost_device;
			}

			// otherwise, exit the program (something's whack)
			exit(1);
		}

		// connect to the BMP capture interface
		res = libusb_claim_interface(handle, 5); // Black Magic Trace Capture

		if(res < 0) {
			fprintf(stderr, "libusb_claim_interface: %s\n", libusb_strerror((enum libusb_error) res));

			// if the device fucked off, search again
			if(res == LIBUSB_ERROR_NO_DEVICE) {
				goto lost_device;
			}

			// otherwise, exit the program (something's whack)
			exit(1);
		}

		// enter an infinite loop of reading from the endpoint
		while(1) {
			// attempt to read from the endpoint
			res = libusb_bulk_transfer(handle, ENDPOINT_UP, dataUp, sizeof(dataUp), &count, 0);

			if (res < 0) {
				fprintf(stderr, "libusb_bulk_transfer: %s\n", libusb_strerror((enum libusb_error) res));

				// if the device fucked off, search again
				if (res == LIBUSB_ERROR_NO_DEVICE) {
					goto lost_device;
				}

				// otherwise, retry
				continue;
			}

			// handle the packet
			for(int i = 0; i < count; i++) {
				usbReadQueue.push(dataUp[i]);
			}

			// fprintf(stderr, "read %d bytes, read queue %lu\n", count, usbReadQueue.size());

			// try to get data out of the queue
			while(usbReadQueue.size() >= 3) {
				unsigned char header1 = usbReadQueue.front(); usbReadQueue.pop();
				unsigned char header2 = usbReadQueue.front(); usbReadQueue.pop();

				if(header1 != 0x02 && header2 != 0x08) {
					fprintf(stderr, "got invalid glob %02x %02x\n", header1, header2);
					continue;
				}

				// print the debug out char
				unsigned char outChar = usbReadQueue.front(); usbReadQueue.pop();
				putchar(outChar);
			}
		}

	// we get down here if the device just disappears
lost_device:
		sleep(1);
	}
}
