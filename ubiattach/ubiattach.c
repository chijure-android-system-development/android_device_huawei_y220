#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

/* linux/mtd/ubi-user.h, kernel 3.4. UBI_IOCATT = _IOW('o', 64, struct). */
struct ubi_attach_req {
	int32_t ubi_num;
	int32_t mtd_num;
	int32_t vid_hdr_offset;
	int8_t padding[12];
};

#define UBI_CTRL_IOC_MAGIC 'o'
#define UBI_IOCATT _IOW(UBI_CTRL_IOC_MAGIC, 64, struct ubi_attach_req)

int main(int argc, char **argv)
{
	struct ubi_attach_req req;
	int fd, mtd;

	if (argc != 2) {
		fprintf(stderr, "usage: ubiattach <mtd-num>\n");
		return 1;
	}

	mtd = atoi(argv[1]);
	fd = open("/dev/ubi_ctrl", O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "ubi_ctrl: %s\n", strerror(errno));
		return 1;
	}

	memset(&req, 0, sizeof(req));
	req.ubi_num = -1;
	req.mtd_num = mtd;
	req.vid_hdr_offset = 0;

	if (ioctl(fd, UBI_IOCATT, &req) < 0) {
		fprintf(stderr, "attach mtd%d: %s\n", mtd, strerror(errno));
		close(fd);
		return 1;
	}

	printf("mtd%d -> ubi%d\n", mtd, req.ubi_num);
	close(fd);
	return 0;
}
