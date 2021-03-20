/*Copyright (C) 2021 Lucie Cupcakes <lucie_linux [at] protonmail.com>
This file is part of qemu-run <https://github.com/lucie-cupcakes/qemu-run>.
qemu-run is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 3, or (at your option) any later version.
qemu-run is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
You should have received a copy of the GNU General Public License
along with qemu-run; see the file LICENSE.  If not see <http://www.gnu.org/licenses/>.*/
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <unistd.h>
#if defined(__WIN32__) || defined(__WIN64__) || defined(__WINNT__) || defined(__DOS__)
#include <limits.h>
#else
#define __NIX__
#include <sys/sysinfo.h>
#include <linux/limits.h>
#endif
#include <sys/stat.h>
#include <sys/types.h>
#include "hashmap.h"

#define buff_size_slice 128
#define buff_size_max buff_size_slice*32

#define hashmap_put_lk(m, lk, val) \
	hashmap_put(m, lk, sizeof(lk)-1, val) // where lk is str literal
#define hashmap_get_lk(m, lk) \
	hashmap_get(m, lk, sizeof(lk)-1) // where k is str literal
#define hashmap_match_char_lka(t, lk, s) \
	(strcasecmp((const char*)hashmap_get(t, lk, sizeof(lk)-1), s) == 0 ? 1 : 0)

#define puts_gpl_banner() \
	puts("qemu-run. Forever beta software. Use on production on your own risk!\nThis software is Free software - released under the GPLv3 License.\nRead the LICENSE file. Or go visit https://www.gnu.org/licenses/gpl-3.0.html\n")

#define path_is_dir(p) (filetype(p) == 1 ? 1 : 0)
#define path_is_file(p) (filetype(p) == 2 ? 1 : 0)

#define append_to_strpool(strpool_ptr, val) do { \
		*strpool_ptr = append_to_char_arr(*strpool_ptr, val, 1, 0, 0); \
	}while(0)

#define append_to_cmd(cmd, val) \
	append_to_char_arr(cmd, val, 0, 1, ' ')

#define add_cfg_val(val_tmp_ptr, str_pool_ptr, cfg, lkey, val) \
	val_tmp_ptr = *str_pool_ptr; \
	append_to_strpool(str_pool_ptr, val); \
	hashmap_put_lk(cfg, lkey, val_tmp_ptr)

char *append_to_char_arr(char *dst, const char *src, bool append_strterm, bool append_endchar, char endchar) {
	char c; bool stop = 0;
	while (!stop) {
		c = *src; src++;
		if (c == '\0' && append_strterm) stop=1;
		if (c == '\0' && !append_strterm) break;
		*dst = c; dst++;
	}
	if (append_endchar) { *dst = endchar; dst++; }
	return dst;
}

int filetype(const char *fpath) {
	int ret = 0; struct stat sb;
	if ((ret=(access (fpath,0) == 0))) {
		stat(fpath,&sb);
		ret+=S_ISREG(sb.st_mode)>0;
	}
	return ret;
}

size_t cstr_trim_right(const char *cstr, const size_t len) {
	size_t diff = 0;
	for (size_t i = len - 1; i < len; i--) {
		if (! isspace((int)cstr[i])) break;
		diff++;
	}
	return len - diff;
}

bool process_kv_pair(char *kv_cstr, struct hashmap_s *cfg, char **str_pool, char delim) {
	bool rc = 0, setting_key = 1, trimming_left = 1;
	char val_buff[buff_size_slice], key_buff[buff_size_slice], *key_ptr, *val_ptr;
	size_t val_len = 0, key_len = 0;
	for (char c = 0; (c = *kv_cstr) != '\0'; kv_cstr++) {
		if (c == delim) { setting_key = 0; trimming_left = 1; continue; }
		if (setting_key && trimming_left && !isspace(c)) {
			trimming_left = 0; key_buff[key_len] = c; key_len++;
		} else if (setting_key && !trimming_left)  {
			key_buff[key_len] = c; key_len++;
		} else if (!setting_key && trimming_left && !isspace(c)) {
			trimming_left = 0; val_buff[val_len] = c; val_len++;
		} else if (!setting_key && !trimming_left) {
			val_buff[val_len] = c; val_len++;
		}
	}
	if (! setting_key) { // If we actually did set a value.
		key_buff[key_len] = '\0'; val_buff[val_len] = '\0';
		key_len = cstr_trim_right(key_buff, key_len);
		val_len = cstr_trim_right(val_buff, val_len);
		key_buff[key_len] = '\0'; val_buff[val_len] = '\0';
		key_ptr = *str_pool; append_to_strpool(str_pool, key_buff);
		val_ptr = *str_pool; append_to_strpool(str_pool, val_buff);
		rc = (hashmap_put(cfg, key_ptr, key_len, val_ptr) != 0);
	}
	return rc;
}

bool program_load_config(char *err, struct hashmap_s *cfg, char **str_pool, const char *fpath) {
	FILE *fptr = fopen(fpath, "r");
	if (fptr == NULL) {
		strcpy(err, "Cannot open config file. Check file permissions?");
		return 0;
	}
	char line[buff_size_slice*2];
	while(fgets(line, buff_size_slice*2, fptr) != NULL) {
		line[strlen(line)-1] = '\0'; // Remove EOF or Newline char
		process_kv_pair(line, cfg, str_pool, '=');
	}
	fclose(fptr);
	return 1;
}

bool program_set_default_cfg_values(struct hashmap_s *cfg, char **str_pool, char *vm_dir) {
	char path_buff[PATH_MAX], nproc_str[4], *val_tmp;
#ifdef __NIX__
	snprintf(nproc_str, 4, "%d", get_nprocs());
#else
   strcpy(nproc_str,"2");
#endif
	add_cfg_val(val_tmp, str_pool, cfg, "sys", "x64");
	add_cfg_val(val_tmp, str_pool, cfg, "efi", "no");
	add_cfg_val(val_tmp, str_pool, cfg, "cpu", "host");
	add_cfg_val(val_tmp, str_pool, cfg, "cores", nproc_str);
	add_cfg_val(val_tmp, str_pool, cfg, "mem", "2G");
	add_cfg_val(val_tmp, str_pool, cfg, "acc", "yes");
	add_cfg_val(val_tmp, str_pool, cfg, "vga", "virtio");
	add_cfg_val(val_tmp, str_pool, cfg, "snd", "hda");
	add_cfg_val(val_tmp, str_pool, cfg, "boot", "c");
	add_cfg_val(val_tmp, str_pool, cfg, "fwd_ports", "2222:22");
	add_cfg_val(val_tmp, str_pool, cfg, "hdd_virtio", "yes");
	add_cfg_val(val_tmp, str_pool, cfg, "net", "virtio-net-pci");
	add_cfg_val(val_tmp, str_pool, cfg, "rng_dev", "yes");
	add_cfg_val(val_tmp, str_pool, cfg, "host_video_acc", "no");
	add_cfg_val(val_tmp, str_pool, cfg, "localtime", "no");
	add_cfg_val(val_tmp, str_pool, cfg, "headless", "no");
	add_cfg_val(val_tmp, str_pool, cfg, "vnc_pwd", "");
	add_cfg_val(val_tmp, str_pool, cfg, "monitor_port", "5510");
	snprintf(path_buff, PATH_MAX, "%s/shared", vm_dir);
	add_cfg_val(val_tmp, str_pool, cfg, "shared", path_is_dir(path_buff) ? path_buff : "");
	snprintf(path_buff, PATH_MAX, "%s/floppy", vm_dir);
	add_cfg_val(val_tmp, str_pool, cfg, "floppy", path_is_file(path_buff) ? path_buff : "");
	snprintf(path_buff, PATH_MAX, "%s/cdrom", vm_dir);
	add_cfg_val(val_tmp, str_pool, cfg, "cdrom", path_is_file(path_buff) ? path_buff : "");
	snprintf(path_buff, PATH_MAX, "%s/disk", vm_dir);
	add_cfg_val(val_tmp, str_pool, cfg, "disk", path_is_file(path_buff) ? path_buff : "");
	return 1;
}

bool program_build_cmd_line(char *err, struct hashmap_s *cfg, char *vm_name, char *out_cmd) {
	bool rc = 1;
	int drive_index = 0, telnet_port = 55555; // @TODO: Get usable TCP port
	void* cfg_v;
	char cmd_slice[buff_size_slice] = {0};

	bool vm_has_name = (strcmp(vm_name, "") != 0 ? 1 : 0);
	bool vm_has_acc_enabled = hashmap_match_char_lka(cfg, "acc", "yes");
	bool vm_has_vncpwd = (strcmp(hashmap_get_lk(cfg, "vnc_pwd"), "") != 0 ? 1 : 0);
	bool vm_has_audio = hashmap_match_char_lka(cfg, "snd", "no");
	bool vm_has_videoacc = hashmap_match_char_lka(cfg, "host_video_acc", "yes");
	bool vm_has_rngdev = hashmap_match_char_lka(cfg, "rng_dev", "yes");
	bool vm_is_headless = hashmap_match_char_lka(cfg, "headless", "yes");
	bool vm_clock_is_localtime = hashmap_match_char_lka(cfg, "localtime", "yes");
	bool vm_has_sharedf = (strcmp(hashmap_get_lk(cfg, "shared"), "") != 0 ? 1 : 0);
	bool vm_has_hddvirtio = hashmap_match_char_lka(cfg, "hdd_virtio", "yes");
	bool vm_has_network = (strcmp(hashmap_get_lk(cfg, "net"), "") != 0 ? 1 : 0);
	vm_has_sharedf = vm_has_sharedf ? path_is_dir(hashmap_get_lk(cfg, "shared")) : 0;

	cfg_v = hashmap_get_lk(cfg, "sys");
	if (strcmp((const char*)cfg_v, "x32") == 0) {
		out_cmd = append_to_cmd(out_cmd, "qemu-system-i386");
	} else if (strcmp((const char*)cfg_v, "x64") == 0) {
		out_cmd = append_to_cmd(out_cmd, "qemu-system-x86_64");
	} else {
		strcpy(err, "Invalid value for sys");
		return 0;
	}

	snprintf(cmd_slice, buff_size_slice, "%s-name %s -cpu %s -smp %s -m %s -boot order=%s -usb -device usb-tablet -vga %s %s%s",
		vm_has_acc_enabled ? "--enable-kvm " : "",
		vm_has_name ? vm_name : "QEMU",
		(char*)hashmap_get_lk(cfg, "cpu"),
		(char*)hashmap_get_lk(cfg, "cores"),
		(char*)hashmap_get_lk(cfg, "mem"),
		(char*)hashmap_get_lk(cfg, "boot"),
		(char*)hashmap_get_lk(cfg, "vga"),
		vm_has_audio ? "-soundhw " : "",
		vm_has_audio ? (char*)hashmap_get_lk(cfg, "snd") : ""
	);
	out_cmd = append_to_cmd(out_cmd, cmd_slice);

	if (vm_is_headless) {
		snprintf(cmd_slice, buff_size_slice, "-display none -monitor telnet:127.0.0.1:%d,server,nowait -vnc %s",
			telnet_port,
			vm_has_vncpwd ? "127.0.0.1:0,password" : "127.0.0.1:0");
		out_cmd = append_to_cmd(out_cmd, cmd_slice);
	} else {
		out_cmd = append_to_cmd(out_cmd, vm_has_videoacc ? "-display gtk,gl=on" : "-display gtk,gl=off");
	}

	if (vm_has_network) {
		snprintf(cmd_slice, buff_size_slice, "-nic user,model=%s%s%s",
			(char*)hashmap_get_lk(cfg, "net"),
			vm_has_sharedf ? ",smb=" : "",
			vm_has_sharedf ? (char*)hashmap_get_lk(cfg, "shared") : ""
		);
		out_cmd = append_to_char_arr(out_cmd, cmd_slice, 0, 0, 0);
		cfg_v = hashmap_get_lk(cfg, "fwd_ports");
		bool vm_has_fwd_ports = (strcmp((const char*) cfg_v, "no") != 0);
		if (! vm_has_fwd_ports) {
			out_cmd = append_to_char_arr(out_cmd, " ", 0, 0, 0);
		} else {
			char* cfg_v_c = (char*)cfg_v;
			if (strstr(cfg_v_c, ":") != NULL) { // If have fwd_ports=<HostPort>:<GuestPort>
				char *fwd_ports_tk = strtok(cfg_v_c, ":");
				char fwd_port_a[16], fwd_port_b[16];
				for (int i = 0; fwd_ports_tk != NULL && i<2; i++) {
					strcpy(i == 0 ? fwd_port_a : fwd_port_b, fwd_ports_tk);
					fwd_ports_tk = strtok(NULL, ":");
				}
				snprintf(cmd_slice, buff_size_slice, ",hostfwd=tcp::%s-:%s,hostfwd=udp::%s-:%s", fwd_port_a, fwd_port_b, fwd_port_a, fwd_port_b);
			} else { // Else use the same port for Host and Guest.
				snprintf(cmd_slice, buff_size_slice, ",hostfwd=tcp::%s-:%s,hostfwd=udp::%s-:%s", cfg_v_c, cfg_v_c, cfg_v_c, cfg_v_c);
			}
			out_cmd = append_to_cmd(out_cmd, cmd_slice);
		}
	}

	cfg_v = hashmap_get_lk(cfg, "floppy");
	if (path_is_file((const char*) cfg_v)) {
		snprintf(cmd_slice, buff_size_slice, "-drive index=%d,file=%s,if=floppy,format=raw", drive_index, (char*)cfg_v);
		out_cmd = append_to_cmd(out_cmd, cmd_slice);
		drive_index++;
	}

	cfg_v = hashmap_get_lk(cfg, "cdrom");
	if (path_is_file((const char*) cfg_v)) {
		snprintf(cmd_slice, buff_size_slice, "-drive index=%d,file=%s,media=cdrom", drive_index, (char*)cfg_v);
		out_cmd = append_to_cmd(out_cmd, cmd_slice);
		drive_index++;
	}

	cfg_v = hashmap_get_lk(cfg, "disk");
	if (path_is_file((const char*) cfg_v)) {
		snprintf(cmd_slice, buff_size_slice, "-drive index=%d,file=%s%s", drive_index, (char*)cfg_v, vm_has_hddvirtio ? ",if=virtio" : "");
		out_cmd = append_to_cmd(out_cmd, cmd_slice);
		drive_index++;
	}

	if (vm_has_rngdev) out_cmd = append_to_cmd(out_cmd, "-object rng-random,id=rng0,filename=/dev/random -device virtio-rng-pci,rng=rng0");
	if (vm_clock_is_localtime) out_cmd = append_to_char_arr(out_cmd, "-rtc base=localtime", 1, 0, 0);
	return rc;
}

bool program_find_vm_location(char *err, int argc, char **argv, char *out_vm_name, char *out_vm_dir, char *out_vm_cfg_file) {
	bool rc = 0, vm_dir_exists = 0;
	char *vm_name, vm_dir[buff_size_slice-16];

	if (argc >= 1) {
		vm_name = argv[1];
	} else {
		strcpy(err, "Invalid argument count. Did you specified the VM Name?");
		return 0;
	}

	char vm_dir_env_str[buff_size_max], *vm_dir_env_delim = ";";
	strcpy(vm_dir_env_str, getenv("QEMURUN_VM_PATH"));

	char *vm_dir_env = strtok(vm_dir_env_str, vm_dir_env_delim);
	while ( vm_dir_env != NULL && vm_dir_exists == 0 ) {
		snprintf(vm_dir, PATH_MAX, "%s/%s", vm_dir_env, vm_name);
		vm_dir_exists = path_is_dir(vm_dir);
		vm_dir_env = strtok(NULL, vm_dir_env_delim);
	}

	if (vm_dir_exists) {
		char cfg_file[PATH_MAX];
		snprintf(cfg_file, PATH_MAX, "%s/config", vm_dir);
		strcpy(out_vm_name, vm_name);
		strcpy(out_vm_dir, vm_dir);
		strcpy(out_vm_cfg_file, cfg_file);
		rc = 1;
	} else {
		strcpy(err, "Cannot find VM, Check your QEMURUN_VM_PATH env. variable ?");
	}

	return rc;
}

int main(int argc, char **argv) {
	char cmd[buff_size_max], err[buff_size_slice], str_pool_d[buff_size_max];
	char vm_name[buff_size_slice], vm_dir[PATH_MAX-16], vm_cfg_file[buff_size_slice];
	char *err_p = &err[0], *str_pool = &str_pool_d[0];
	struct hashmap_s cfg;
	puts_gpl_banner();
	if (hashmap_create(16, &cfg) == 0 &&
		program_find_vm_location(err_p, argc, argv, vm_name, vm_dir, vm_cfg_file) &&
		program_set_default_cfg_values(&cfg, &str_pool, vm_dir) &&
		program_load_config(err_p, &cfg, &str_pool, vm_cfg_file) &&
		program_build_cmd_line(err_p, &cfg, vm_name, cmd))
	{
		puts("Command line arguments:");
		puts(cmd);
		system(cmd);
	} else {
		puts("There was an error in the program:");
		puts(err);
	}
	hashmap_destroy(&cfg);
	return 0;
}
