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
#if defined(__unix__) || defined(__unix) || defined (unix)
#define __NIX__
#define PSEP ":"
#define DSEP "/"
#include <unistd.h>
#include <linux/limits.h>
#else
#define PSEP ";"
#define DSEP "\\"
#include <limits.h>
#endif
#include <sys/stat.h>
#include <sys/types.h>

#ifdef DEBUG
#ifdef __GNUC__
#define dprint() printf("%s (%d) %s\n",__FILE__,__LINE__,__FUNCTION__);
#else
#define dprint() printf("%s (%d) %s\n",__FILE__,__LINE__);
#endif
#else
#define dprint()
#endif // DEBUG

#ifndef LINE_MAX
#define LINE_MAX (512)
#endif


#define puts_gpl_banner()                                              \
	puts("qemu-run. Forever beta software. Use on production on your own risk!\nThis software is Free software - released under the GPLv3 License.\nRead the LICENSE file. Or go visit https://www.gnu.org/licenses/gpl-3.0.html\n")

#include "config.h"

enum {ERR_UNKOWN,ERR_ARGS,ERR_ENV,ERR_CONFIG,ERR_SYS,ERR_EXEC,ERR_ENDLIST};

void fatal(unsigned int errcode)
{
   char *errs[]={
      "Invalid argument count. Did you specified the VM Name?",
      "Cannot find VM, Check your QEMURUN_VM_PATH env. variable ?",
      "Cannot open config file. Check file permissions?",
      "Invalid value for sys",
      "There was an error trying to execute qemu. Do you have it installed?."
   };

   printf("There was an error in the program:\n\t%s\n",errs[errcode<ERR_ENDLIST?errcode:0]);
   exit(1);
}
long sym_hash_generate(char *str)
{
   int pos=0;
   long hash=0xCAFEBABE;

   while(str[pos]) {
      hash=~(((((hash&0xFF)^str[pos])<<5)|
         (((hash>>26)&0x1f)^(pos&31)))|
         (hash<<18));
      pos++;
   }
   return hash;
}

bool sym_put_kv(char *key,char *val)
{
   long hash,cnt=0,ret=0;

   hash=sym_hash_generate(key);
   while(cnt<KEY_ENDLIST) {
      if(hash==cfg[cnt].hash) {
         ret=1;
         cfg[cnt].val=strdup(val);
      }
   }
   return ret;
}

char *append_to_char_arr(char *dst, const char *src, bool append_strterm, bool append_endchar, char endchar) {
	char c; bool stop = 0;
	dprint();
	while (!stop) {
		c = *src; src++;
		if (c == '\0' && append_strterm) stop=1;
		if (c == '\0' && !append_strterm) break;
		*dst = c; dst++;
	}
	if (append_endchar) { *dst = endchar; dst++; }
	return dst;
}

enum {FT_TYPE,FT_UNKNOWN=0,FT_PATH,FT_FILE};
int filetype(const char *fpath,int type) {
	int ret = 0; struct stat sb;
	dprint();
	if ((ret=(access (fpath,0) == 0))) {
		stat(fpath,&sb);
		ret=S_ISREG(sb.st_mode)>0?FT_FILE:(S_ISDIR(sb.st_mode)?FT_PATH:FT_UNKNOWN);
	}
	return ret=(type)?ret==type:ret;
}

size_t cstr_trim_right(const char *cstr, const size_t len) {
	size_t diff = 0;
	dprint();
	for (size_t i = len - 1; i < len; i--) {
		if (! isspace((int)cstr[i])) break;
		diff++;
	}
	return len - diff;
}

bool process_kv_pair(char *kv_cstr, char delim) {
	bool rc = 0, setting_key = 1, trimming_left = 1;
	char val_buff[LINE_MAX], key_buff[LINE_MAX], *key_ptr, *val_ptr;
	size_t val_len = 0, key_len = 0;
   long hash;

	dprint();
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
      hash=sym_hash_generate(key_buff);
		rc = (sym_put_kv(key_ptr, val_ptr) != 0);
	}
	return rc;
}

bool program_load_config(const char *fpath) {
	FILE *fptr = fopen(fpath, "r");
	dprint();
	if (fptr == NULL)
      fatal(ERR_CONFIG);
	char line[LINE_MAX*2];
	while(fgets(line, LINE_MAX*2, fptr) != NULL) {
		line[strlen(line)-1] = '\0'; // Remove EOF or Newline char
		process_kv_pair(line, '=');
	}
	fclose(fptr);
	return 1;
}

bool program_set_default_cfg_values(char *vm_dir) {
	char path_buff[PATH_MAX], nproc_str[4];
	dprint();
#ifdef __NIX__
	snprintf(nproc_str, 4, "%d", get_nprocs());
#else
   strcpy(nproc_str,"2");
#endif
	sym_put_kv("sys", "x64");
	sym_put_kv("efi", "no");
	sym_put_kv("cpu", "host");
	sym_put_kv("cores", nproc_str);
	sym_put_kv("mem", "2G");
	sym_put_kv("acc", "yes");
	sym_put_kv("vga", "virtio");
	sym_put_kv("snd", "hda");
	sym_put_kv("boot", "c");
	sym_put_kv("fwd_ports", "2222:22");
	sym_put_kv("hdd_virtio", "yes");
	sym_put_kv("net", "virtio-net-pci");
	sym_put_kv("rng_dev", "yes");
	sym_put_kv("host_video_acc", "no");
	sym_put_kv("localtime", "no");
	sym_put_kv("headless", "no");
	sym_put_kv("vnc_pwd", "");
	sym_put_kv("monitor_port", "5510");
	snprintf(path_buff, PATH_MAX, "%s/shared", vm_dir);
	sym_put_kv("shared", filetype(path_buff,FT_PATH) ? path_buff : "");
	snprintf(path_buff, PATH_MAX, "%s/floppy", vm_dir);
	sym_put_kv("floppy", filetype(path_buff,FT_FILE) ? path_buff : "");
	snprintf(path_buff, PATH_MAX, "%s/cdrom", vm_dir);
	sym_put_kv("cdrom", filetype(path_buff,FT_FILE) ? path_buff : "");
	snprintf(path_buff, PATH_MAX, "%s/disk", vm_dir);
	sym_put_kv("disk", filetype(path_buff,FT_FILE) ? path_buff : "");
	return 1;
}

bool program_build_cmd_line(char *vm_name, char *out_cmd) {
	bool rc = 1;
	int drive_index = 0, telnet_port = 55555; // @TODO: Get usable TCP port
	char cmd_slice[LINE_MAX] = {0};
   dprint();
	bool vm_has_name = (strcmp(vm_name, "") != 0 ? 1 : 0);
	bool vm_has_acc_enabled = strcmpi(cfg[KEY_ACC].val, "yes")==0;
	bool vm_has_vncpwd = (strcmp(cfg[KEY_VNC_PWD].val, "") != 0 ? 1 : 0);
	bool vm_has_audio = strcmpi(cfg[KEY_SND].val, "no");
	bool vm_has_videoacc = strcmpi(cfg[KEY_HOST_VIDEO_ACC].val, "yes");
	bool vm_has_rngdev = strcmpi(cfg[KEY_RNG_DEV].val, "yes");
	bool vm_is_headless = strcmpi(cfg[KEY_HEADLESS].val, "yes");
	bool vm_clock_is_localtime = strcmpi(cfg[KEY_LOCALTIME].val, "yes");
	bool vm_has_sharedf = (strcmp(cfg[KEY_SHARED].val, "") != 0 ? 1 : 0);
	bool vm_has_hddvirtio = strcmpi(cfg[KEY_HDD_VIRTIO].val, "yes");
	bool vm_has_network = (strcmp(cfg[KEY_NET].val, "") != 0 ? 1 : 0);
	vm_has_sharedf = vm_has_sharedf ? filetype(cfg[KEY_SHARED].val,FT_PATH) : 0;

   *out_cmd=0;
	if (strcmp(cfg[KEY_SYS].val, "x32") == 0) {
		out_cmd = strcpy(out_cmd, "qemu-system-i386");
	} else if (strcmp(cfg[KEY_SYS].val, "x64") == 0) {
		out_cmd = strcpy(out_cmd, "qemu-system-x86_64");
	} else
      fatal(ERR_SYS);

	snprintf(cmd_slice, LINE_MAX, "%s-name %s -cpu %s -smp %s -m %s -boot order=%s -usb -device usb-tablet -vga %s %s%s",
		vm_has_acc_enabled ? "--enable-kvm " : "",
		vm_has_name ? vm_name : "QEMU",
		cfg[KEY_CPU].val,
		cfg[KEY_CORES].val,
		cfg[KEY_MEM].val,
		cfg[KEY_BOOT].val,
		cfg[KEY_VGA].val,
		vm_has_audio ? "-soundhw " : "",
		vm_has_audio ? cfg[KEY_SND].val : ""
	);
	strcat(out_cmd, cmd_slice);

	if (vm_is_headless) {
		snprintf(cmd_slice, LINE_MAX, "-display none -monitor telnet:127.0.0.1:%d,server,nowait -vnc %s",
			telnet_port,
			vm_has_vncpwd ? "127.0.0.1:0,password" : "127.0.0.1:0");
		strcat(out_cmd, cmd_slice);
	} else {
		strcat(out_cmd, vm_has_videoacc ? "-display gtk,gl=on" : "-display gtk,gl=off");
	}

	if (vm_has_network) {
		snprintf(cmd_slice, LINE_MAX, "-nic user,model=%s%s%s",
			cfg[KEY_NET].val,
			vm_has_sharedf ? ",smb=" : "",
			vm_has_sharedf ? cfg[KEY_SHARED].val : ""
		);
		out_cmd = append_to_char_arr(out_cmd, cmd_slice, 0, 0, 0);
		bool vm_has_fwd_ports = (strcmp(cfg[KEY_FWD_PORTS].val, "no") != 0);
		if (! vm_has_fwd_ports) {
			out_cmd = append_to_char_arr(out_cmd, " ", 0, 0, 0);
		} else {
			char* cfg_v_c = cfg[KEY_FWD_PORTS].val;
			if (strstr(cfg[KEY_FWD_PORTS].val, ":") != NULL) { // If have fwd_ports=<HostPort>:<GuestPort>
				char *fwd_ports_tk = strtok(cfg[KEY_FWD_PORTS].val, ":");
				char fwd_port_a[16], fwd_port_b[16];
				for (int i = 0; fwd_ports_tk != NULL && i<2; i++) {
					strcpy(i == 0 ? fwd_port_a : fwd_port_b, fwd_ports_tk);
					fwd_ports_tk = strtok(NULL, ":");
				}
				snprintf(cmd_slice, LINE_MAX, ",hostfwd=tcp::%s-:%s,hostfwd=udp::%s-:%s", fwd_port_a, fwd_port_b, fwd_port_a, fwd_port_b);
			} else { // Else use the same port for Host and Guest.
				snprintf(cmd_slice, LINE_MAX, ",hostfwd=tcp::%s-:%s,hostfwd=udp::%s-:%s", cfg_v_c, cfg_v_c, cfg_v_c, cfg_v_c);
			}
			strcat(out_cmd, cmd_slice);
		}
	}

	if (filetype((const char*) cfg[KEY_FLOPPY].val,FT_FILE)) {
		snprintf(cmd_slice, LINE_MAX, "-drive index=%d,file=%s,if=floppy,format=raw", drive_index, cfg[KEY_FLOPPY].val);
		strcat(out_cmd, cmd_slice);
		drive_index++;
	}

	if (filetype(cfg[KEY_CDROM].val,FT_FILE)) {
		snprintf(cmd_slice, LINE_MAX, "-drive index=%d,file=%s,media=cdrom", drive_index, cfg[KEY_CDROM].val);
		strcat(out_cmd, cmd_slice);
		drive_index++;
	}

	if (filetype(cfg[KEY_DISK].val,FT_FILE)) {
		snprintf(cmd_slice, LINE_MAX, "-drive index=%d,file=%s%s", drive_index, cfg[KEY_DISK].val, vm_has_hddvirtio ? ",if=virtio" : "");
		strcat(out_cmd, cmd_slice);
		drive_index++;
	}

	if (vm_has_rngdev) strcat(out_cmd, "-object rng-random,id=rng0,filename=/dev/random -device virtio-rng-pci,rng=rng0");
	if (vm_clock_is_localtime) out_cmd = append_to_char_arr(out_cmd, "-rtc base=localtime", 1, 0, 0);
	return rc;
}

bool program_find_vm_location(int argc, char **argv, char *out_vm_name, char *out_vm_dir, char *out_vm_cfg_file) {
	bool rc = 0, vm_dir_exists = 0;
	char *vm_name, vm_dir[PATH_MAX+1];

   dprint();
	if (argc > 1) {
		vm_name = argv[1];
	} else
      fatal(ERR_ARGS);

	char vm_dir_env_str[PATH_MAX*16],*env;

	strcpy(vm_dir_env_str, (const char *)((env=getenv("QEMURUN_VM_PATH"))?env:""));

	char *vm_dir_env = strtok(vm_dir_env_str, PSEP);
	while ( vm_dir_env != NULL && vm_dir_exists == 0 ) {
		snprintf(vm_dir, PATH_MAX, "%s"DSEP"%s", vm_dir_env, vm_name);
		vm_dir_exists = filetype(vm_dir,FT_PATH);
		vm_dir_env = strtok(NULL, PSEP);
	}
	if (vm_dir_exists) {
		char cfg_file[PATH_MAX+1];
		snprintf(cfg_file, PATH_MAX, "%s"DSEP"config", vm_dir);
		strcpy(out_vm_name, vm_name);
		strcpy(out_vm_dir, vm_dir);
		strcpy(out_vm_cfg_file, cfg_file);
		rc = 1;
	} else
      fatal(ERR_ENV);
	return rc;
}

int main(int argc, char **argv) {
	char cmd[LINE_MAX], str_pool_d[LINE_MAX];
	char vm_name[LINE_MAX], vm_dir[PATH_MAX+1], vm_cfg_file[LINE_MAX];
	char *str_pool = &str_pool_d[0];

	dprint();
	puts_gpl_banner();
	if (program_find_vm_location(argc, argv, vm_name, vm_dir, vm_cfg_file) &&
		program_set_default_cfg_values(vm_dir) &&
		program_load_config(vm_cfg_file) &&
		program_build_cmd_line(vm_name, cmd))
	{
		puts("QEMU Command line arguments:");
		puts(cmd);
		if (system(cmd) == -1)
         fatal(ERR_EXEC);
	}
	return 0;
}
