struct scsi_cmd {
	unsigned long size;
	unsigned char *buf;
	unsigned char cmd_cnt;
	unsigned char *cmd;
};

#define SCSI_READ	(('S'<<8)|'R')
#define SCSI_WRITE	(('S'<<8)|'W')

/* Device types */
#define SCSI_DISK	0
#define SCSI_TAPE	1
#define SCSI_PRINTER	2
#define SCSI_CPU	3
#define SCSI_WROM	4
#define SCSI_CDROM	5

struct dev_setup {
	unsigned char scsi_lun;
	unsigned char scsi_dev;
};
