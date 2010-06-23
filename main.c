/// (C) Gojohnnyboi, msftguy 2010

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <IOKit/IOKitLib.h>

io_service_t get_io_service(const char *name) {
	CFMutableDictionaryRef matching;
	io_service_t service;
	
	matching = IOServiceMatching(name);
	if(matching == NULL) {
		printf("unable to create matching dictionary for class '%s'", name);
		return 0;
	}
	
	while(!service) {
		CFRetain(matching);
		service = IOServiceGetMatchingService(kIOMasterPortDefault, matching);
		if(service) break;
		
		printf("waiting for matching IOKit service: %s\n", name);
		sleep(1);
		CFRelease(matching);
	}
	
	CFRelease(matching);
	
	return service;
}

int img3_flash_NOR_image(io_connect_t norServiceConnection, const char* filename, int isLLB) {
	printf("[INFO] %s: flashing %s data\n", "img3_flash_NOR_image", (isLLB ? "LLB" : "NOR"));
	
	int fd = open(filename, O_RDONLY);
	size_t imgLen = lseek(fd, 0, SEEK_END);
	printf("imgLen=%u\n", imgLen);
	lseek(fd, 0, SEEK_SET);
	void *mappedImage = mmap(NULL, imgLen, PROT_READ | PROT_WRITE, MAP_ANON | VM_FLAGS_PURGABLE, -1, 0);
	if(mappedImage == MAP_FAILED) {
		int err = errno;
		printf("mmap (size = %d) failed: %s\n", imgLen, strerror(err));
		return err;
	}
	
	int cbRead = read(fd, mappedImage, imgLen);
	if (cbRead != imgLen) {
		int err = errno;
		printf("cbRead(%u) != imgLen(%u); err 0x%x\n", cbRead, imgLen, err);
		return err;
	}
	
	kern_return_t result;
	if((result = IOConnectCallStructMethod(norServiceConnection, isLLB ? 0 : 1, mappedImage, imgLen, NULL, 0)) != KERN_SUCCESS) {
		printf("IOConnectCallStructMethod failed: 0x%x\n", result);
	}
	
	munmap(mappedImage, imgLen);
	
	return result;
}


int main(int argc, const char **argv) {
	if (argc == 1) {
		printf("Usage: %s LLB.img3 [iboot.img3 [devicetree.img3 [...]]]\n", *argv);
		exit(0);
	}
	mach_port_t     masterPort;
	
	kern_return_t k_result = IOMasterPort(MACH_PORT_NULL, &masterPort);
	if (k_result)
	{
		printf("IOMasterPort failed: 0x%X\n", k_result);
		exit(7);
	}
	printf("[OK] IOMasterPort opened\n");

	io_service_t norService = get_io_service("AppleImage3NORAccess");
	if (norService == 0) {
		printf("get_io_service failed!\n");
		exit(2);
	}
	printf("[OK] AppleImage3NORAccess found: 0x%x\n", norService);
	
	io_connect_t norServiceConnection; 
	k_result = IOServiceOpen(norService, mach_task_self_, 0, &norServiceConnection);
	if (k_result != KERN_SUCCESS) {
		printf("IOServiceOpen failed: 0x%X\n", k_result);
		exit(5);		
	}
	printf("[OK] IOServiceOpen: conn = 0x%x\n", norServiceConnection);

	for (int i = 1; i < argc; ++i) {
		const char* filename = argv[i];

		bool isLLB = !!strcasestr(filename, "llb");
		int result = img3_flash_NOR_image(norServiceConnection, filename, isLLB);
		if (result != 0) {
			printf("[FAIL] img3_flash_NOR_image(%s), ERROR = 0x%X\n", filename, result);
			exit(3);			
		}
		printf("[OK] Flashing %s\n", filename);
	}
	IOServiceClose(norServiceConnection);
	IOObjectRelease(norService);
	printf("[OK] SUCCESS\n", norServiceConnection);
	return 0;
}