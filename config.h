enum { KEY_VM_NAME,KEY_SYS,KEY_EFI,KEY_CPU,KEY_CORES,KEY_MEM,KEY_ACC,KEY_VGA,KEY_SND,KEY_BOOT,KEY_FWD_PORTS,KEY_HDD_VIRTIO,KEY_NET,KEY_RNG_DEV,KEY_HOST_VIDEO_ACC,KEY_LOCALTIME,KEY_HEADLESS,KEY_VNC_PWD,KEY_MONITOR_PORT,KEY_SHARED,KEY_FLOPPY,KEY_CDROM,KEY_DISK,KEY_ENDLIST };

typedef struct {
	int hash;
	char *val;
} st_config;

st_config cfg[24] = {
	{0x1113E420, ""},		/* vm_name */
	{0x1A13FEE4, "x64"},		/* sys */
	{0x7593E1A6, "no"},		/* efi */
	{0x1E93FA26, "host"},		/* cpu */
	{0x519FE282, "2"},		/* cores */
	{0x7413ED27, "2G"},		/* mem */
	{0x3713F4E6, "yes"},		/* acc */
	{0x4513E4A4, "virtio"},		/* vga */
	{0x1193E004, "hda"},		/* snd */
	{0x6A67FDBC, "c"},		/* boot */
	{0x7D2BE73B, "2222:22"},		/* fwd_ports */
	{0x16B3F869, "yes"},		/* hdd_virtio */
	{0x4413EE07, "virtio-pci-net"},		/* net */
	{0x408BEA86, "yes"},		/* rng_dev */
	{0x046FEF14, "no"},		/* host_video_acc */
	{0x7ED7FA17, "no"},		/* localtime */
	{0x18E7E94D, "no"},		/* headless */
	{0x09CBE2C6, ""},		/* vnc_pwd */
	{0x52AFFBE2, "5510"},		/* monitor_port */
	{0x2E8FF8E0, ""},		/* shared */
	{0x2E4BFD78, ""},		/* floppy */
	{0x549BE96E, ""},		/* cdrom */
	{0x0467EE44, ""}		/* disk */
};