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
#include <ctype.h>

#if defined(__unix__) || defined(__unix) || defined(unix)
#define __NIX__
#define PSEP ":"
#define DSEP "/"
#ifdef __linux__
#include <unistd.h>
#include <linux/limits.h>
#else
#include <limits.h>
#endif // __linux__
#else
#define __WINDOWS__
#define PSEP ";"
#define DSEP "\\"
#include <io.h>
#include <limits.h>
#endif // __NIX__
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
#define LINE_AVG (128)
#define LINE_MAX (LINE_AVG*32)
#endif

#ifndef stricmp //GCC is weird sometimes it doesn't includes this..
#include <strings.h>
#define stricmp(x, y) strcasecmp(x, y)
#endif

#define puts_gpl_banner()                                              \
	puts("qemu-run. Forever beta software. Use on production on your own risk!\nThis software is Free software - released under the GPLv3 License.\nRead the LICENSE file. Or go visit https://www.gnu.org/licenses/gpl-3.0.html\n")

#include "config.h"

#if defined(_SVID_SOURCE) || defined(_BSD_SOURCE) || _XOPEN_SOURCE >= 500 || defined(_XOPEN_SOURCE) && defined(_XOPEN_SOURCE_EXTENDED)
#else
char *strdup(const char *s) {
    size_t n = strlen(s)+1;
    char *p = malloc(n);
    if(p) memcpy(p, s, n);
    //printf("strdup: source=%s, len=%lu, ret=%s\n", s, (unsigned long)n, p);
    return p;
}
#endif

enum {ERR_UNKOWN,ERR_ARGS,ERR_ENV,ERR_OPEN_CONFIG,ERR_FIND_CONFIG,ERR_SYS,ERR_EXEC,ERR_ENDLIST};
void fatal(unsigned int errcode) {
	char *errs[]={
		"Unkown error.",
		"Invalid argument count. Did you specified the VM Name?",
		"Cannot find VM, Check your QEMURUN_VM_PATH env. variable?",
		"Cannot find VM config file. Did you created it?",
		"Cannot open config file. Check file permissions?",
		"Config file has an invalid value for sys.",
		"There was an error trying to execute qemu. Do you have it installed?"
	};
	dprint();
	printf("There was an error in the program:\n\t%s.\n",errs[errcode<ERR_ENDLIST?errcode:0]);
	exit(1);
}

void sym_hash_generate(char *str, char *hash_s) {
	int pos=0; long hash=0xCAFEBABE;
	while(str[pos]) {
		hash=~(((((hash&0xFF)^str[pos])<<5)|
			(((hash>>26)&0x1f)^(pos&31)))|
			(hash<<18));
		pos++;
	}
	snprintf(hash_s, 9, "%8X", hash);
	if(hash_s[0]==' ') { hash_s[0]='X';}
}

bool sym_put_kv(char *key,char *val) {
	int cnt=0,ret=0;
	char hash[9]={0};
	sym_hash_generate(key, &hash[0]);
	while(cnt<KEY_ENDLIST) {
		if(strcmp(cfg[cnt].hash, hash) == 0) {
			ret=1;
			cfg[cnt].val=strdup(val);
		}
		cnt++;
	}
	return ret;
}

enum {FT_TYPE,FT_UNKNOWN=0,FT_PATH,FT_FILE};
int filetype(const char *fpath,int type) {
	int ret = 0; struct stat sb;
	if ((ret=(access (fpath,0) == 0))) {
		stat(fpath,&sb);
		ret=S_ISREG(sb.st_mode)>0?FT_FILE:(S_ISDIR(sb.st_mode)?FT_PATH:FT_UNKNOWN);
	}
	return ret=(type)?ret==type:ret;
}

char* cstr_remove_quotes(char* c) {
	dprint();
	size_t len = strlen(c);
	if (c[len-1] == '\"' && c[0] == '\"') {
		c[len-1] = '\0';
		return c+1;
	} else { return c; }
}

#ifdef __WINDOWS__ // For now I only need this function on Windows.
bool get_binary_full_path(const char *bin_fname, char *out_bin_fpath, char *out_dir) {
	dprint();
	bool found = 0;
	char fpath_b[PATH_MAX], env_b[PATH_MAX*16], *env;
	strcpy(env_b, (const char *)((env=getenv("PATH"))?env:""));
	char *dir_p = strtok(env_b, PSEP);
	while (dir_p && !found) {
		char* dir_pq = cstr_remove_quotes(dir_p);
		snprintf(fpath_b, sizeof(fpath_b), "%s"DSEP"%s", dir_pq, bin_fname); found = filetype(fpath_b, FT_FILE);
		if (!found) { snprintf(fpath_b, sizeof(fpath_b), "%s"DSEP"%s.exe", dir_pq, bin_fname); found = filetype(fpath_b, FT_FILE); }
		if (!found) { snprintf(fpath_b, sizeof(fpath_b), "%s"DSEP"%s.com", dir_pq, bin_fname); found = filetype(fpath_b, FT_FILE); }
		if (!found) { snprintf(fpath_b, sizeof(fpath_b), "%s"DSEP"%s.bat", dir_pq, bin_fname); found = filetype(fpath_b, FT_FILE); }
		if (found && out_dir) { strcpy(out_dir, dir_pq); }
		dir_p = strtok(NULL, PSEP);
	}
	if (found && out_bin_fpath) { strcpy(out_bin_fpath, fpath_b); }
	return found;
}
#endif

void program_load_config(const char *fpath) {
	dprint();
	FILE *fptr = fopen(fpath, "r");
#ifdef DEBUG
	printf("fpath=%s\n",fpath);
#endif
	if (!fptr) { fatal(ERR_OPEN_CONFIG); }
	char line[LINE_AVG*2] = {0}, key[LINE_AVG]={0}, val[LINE_AVG]={0};
	while(fgets(line, LINE_AVG*2, fptr)) {
		if(!strchr(line, '=') || line[0] == '#') { continue; }
		size_t len = strlen(line);
		if (len && line[len-2] == '\r') { len--; line[len-1] = '\0'; } 
		if (len && line[len-1] == '\n') { len--; line[len] = '\0'; }
		if (len<3) { continue; }
		char* slice = strtok(line, "=");
		for(int i = 0; slice && i<2; i++) {
			if (i==0) { strcpy(key, slice); }
			if (i==1) { strcpy(val, slice); break; }
			slice = strtok(NULL, "=");
		}
		sym_put_kv(key, val);
	}
	fclose(fptr);
}

void program_set_default_cfg_values() {
	dprint();
#ifdef __WINDOWS__
	sym_put_kv("acc", "no");
	sym_put_kv("cpu", "max");
	sym_put_kv("rng_dev", "no");
#endif
	/* Because now qemu-run chdirs into vm_dir,
	it's not needed to append vm_dir to the filename. */
	if (filetype("shared",FT_PATH))			{ sym_put_kv("shared", "shared"); }
	if (filetype("floppy", FT_FILE)) 		{ sym_put_kv("floppy", "floppy"); }
	if (filetype("floppy.img", FT_FILE)) 	{ sym_put_kv("floppy", "floppy.img"); }
	if (filetype("cdrom", FT_FILE))			{ sym_put_kv("cdrom", "cdrom"); }
	if (filetype("cdrom.iso", FT_FILE))		{ sym_put_kv("cdrom", "cdrom.iso"); }
	if (filetype("disk", FT_FILE))			{ sym_put_kv("disk", "disk"); }
	if (filetype("disk.qcow2", FT_FILE))	{ sym_put_kv("disk", "disk.qcow2"); }
	if (filetype("disk.raw", FT_FILE))		{ sym_put_kv("disk", "disk.raw"); }
	if (filetype("disk.img", FT_FILE))		{ sym_put_kv("disk", "disk.img"); }
}

void program_build_cmd_line(char *vm_name, char *out_cmd) {
	int drive_index = 0, telnet_port = 55555; // @TODO: Get usable TCP port
	char cmd_slice[LINE_MAX] = {0};
	dprint();
#ifdef __WINDOWS__
	char* cmd_sp = out_cmd; // Need this variable, for a Windows fix..
#else
	bool vm_has_rngdev = stricmp(cfg[KEY_RNG_DEV].val, "yes")==0;
#endif
	bool vm_has_name = (strcmp(vm_name, "") != 0 );
	bool vm_has_acc_enabled = stricmp(cfg[KEY_ACC].val, "yes")==0;
	bool vm_has_vncpwd = (strcmp(cfg[KEY_VNC_PWD].val, "") != 0);
	bool vm_has_audio = stricmp(cfg[KEY_SND].val, "no")!=0;
	bool vm_has_videoacc = stricmp(cfg[KEY_HOST_VIDEO_ACC].val, "yes")==0;
	bool vm_is_headless = stricmp(cfg[KEY_HEADLESS].val, "yes")==0;
	bool vm_clock_is_localtime = stricmp(cfg[KEY_LOCALTIME].val, "yes")==0;
	bool vm_has_sharedf = (strcmp(cfg[KEY_SHARED].val, "") != 0);
	bool vm_has_hddvirtio = stricmp(cfg[KEY_HDD_VIRTIO].val, "yes")==0;
	bool vm_has_network = (strcmp(cfg[KEY_NET].val, "") != 0 );
	vm_has_sharedf = vm_has_sharedf ? filetype(cfg[KEY_SHARED].val,FT_PATH) : 0;
	*out_cmd=0;
	if (strcmp(cfg[KEY_SYS].val, "x32") == 0) {
		out_cmd = strcpy(out_cmd, "qemu-system-i386");
	} else if (strcmp(cfg[KEY_SYS].val, "x64") == 0) {
		out_cmd = strcpy(out_cmd, "qemu-system-x86_64");
	} else { fatal(ERR_SYS); }
	snprintf(cmd_slice, LINE_MAX, " %s-name %s -cpu %s -smp %s -m %s -boot order=%s -usb -device usb-tablet -vga %s %s%s",
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
		snprintf(cmd_slice, LINE_MAX, " -display none -monitor telnet:127.0.0.1:%d,server,nowait -vnc %s",
			telnet_port,
			vm_has_vncpwd ? "127.0.0.1:0,password" : "127.0.0.1:0");
		strcat(out_cmd, cmd_slice);
	} else {
		strcat(out_cmd, vm_has_videoacc ? " -display gtk,gl=on" : " -display gtk,gl=off");
	}

	if (vm_has_network) {
		snprintf(cmd_slice, LINE_MAX, " -nic user,model=%s%s%s",
			cfg[KEY_NET].val,
			vm_has_sharedf ? ",smb=" : "",
			vm_has_sharedf ? cfg[KEY_SHARED].val : ""
		);
		out_cmd = strcat(out_cmd, cmd_slice);
		bool vm_has_fwd_ports = (strcmp(cfg[KEY_FWD_PORTS].val, "no") != 0);
		if (! vm_has_fwd_ports) {
			out_cmd = strcat(out_cmd, " ");
		} else {
			char* cfg_v_c = strdup(cfg[KEY_FWD_PORTS].val);
			if (strchr(cfg_v_c, ':') != NULL) { // If have fwd_ports=<HostPort>:<GuestPort>
				char *fwd_ports_tk = strtok(cfg_v_c, ":");
				char fwd_port_a[16], fwd_port_b[16];
				for (int i = 0; fwd_ports_tk && i<2; i++) {
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
		snprintf(cmd_slice, LINE_MAX, " -drive index=%d,file=%s,if=floppy,format=raw", drive_index, cfg[KEY_FLOPPY].val);
		strcat(out_cmd, cmd_slice);
		drive_index++;
	}

	if (filetype(cfg[KEY_CDROM].val,FT_FILE)) {
		snprintf(cmd_slice, LINE_MAX, " -drive index=%d,file=%s,media=cdrom", drive_index, cfg[KEY_CDROM].val);
		strcat(out_cmd, cmd_slice);
		drive_index++;
	}

	if (filetype(cfg[KEY_DISK].val,FT_FILE)) {
		snprintf(cmd_slice, LINE_MAX, " -drive index=%d,file=%s%s", drive_index, cfg[KEY_DISK].val, vm_has_hddvirtio ? ",if=virtio" : "");
		strcat(out_cmd, cmd_slice);
		drive_index++;
	}

#ifdef __WINDOWS__
	/* QEMU on Windows needs, for some reason, to have an additional argument with the program path on it,
	* For example if you have it on: "C:\Program Files\Qemu", You have to run it like this: qemu-system-i386.exe -L "C:\Program Files\Qemu"
	* Otherwise it wont find the BIOS file.. */
	char q_fp[LINE_AVG], q_fn[LINE_AVG], wfix_arg[LINE_AVG+6]={0};
	size_t q_fn_l = 0;
	dprint();
	for (; cmd_sp[q_fn_l] && cmd_sp[q_fn_l] != ' '; q_fn_l++) { q_fn[q_fn_l] = cmd_sp[q_fn_l]; } // Copy the qemu exe file name
	q_fn[q_fn_l] = 0;
	if (! get_binary_full_path(q_fn, NULL, &q_fp[0])) { fatal(ERR_EXEC); } // Grab Qemu EXE file path, using PATH env.
	snprintf(wfix_arg, sizeof(wfix_arg), " -L \"%s\"", q_fp);
	strcat(out_cmd, wfix_arg); // Appened the command line argument.
#else
	if (vm_has_rngdev) strcat(out_cmd, " -object rng-random,id=rng0,filename=/dev/random -device virtio-rng-pci,rng=rng0");
#endif
	if (vm_clock_is_localtime) out_cmd = strcat(out_cmd, " -rtc base=localtime");
}

void program_find_vm_and_chdir(int argc, char **argv, char *out_vm_name, char *out_vm_cfg_file) {
	char vm_dir[PATH_MAX+1], *env, *env_dir;
	bool vm_dir_exists = 0, cfg_file_exists = 0;
	dprint();
	if (argc < 2) { fatal(ERR_ARGS); }
	strcpy(out_vm_name, argv[1]);
	env = getenv("QEMURUN_VM_PATH");
	if (!env) { fatal(ERR_ENV); }
	env_dir = strtok(env, PSEP);
	while ( env_dir && !vm_dir_exists ) {
		char* env_dir_q = cstr_remove_quotes(env_dir);
		snprintf(vm_dir, PATH_MAX, "%s"DSEP"%s", env_dir_q, out_vm_name);
		vm_dir_exists = filetype(vm_dir,FT_PATH);
		env_dir = strtok(NULL, PSEP);
	}
	if (! vm_dir_exists) { fatal(ERR_ENV); }
	chdir(vm_dir);
	strcpy(out_vm_cfg_file, "config"); cfg_file_exists = filetype(out_vm_cfg_file, FT_FILE);
	if (! cfg_file_exists) { strcpy(out_vm_cfg_file, "config.ini"); cfg_file_exists = filetype(out_vm_cfg_file, FT_FILE); }
	if (! cfg_file_exists) { fatal(ERR_FIND_CONFIG); }
}

int main(int argc, char **argv) {
	char cmd[LINE_MAX], vm_name[LINE_AVG], vm_cfg_file[LINE_AVG];
	dprint();
	puts_gpl_banner();
	program_find_vm_and_chdir(argc, argv, vm_name, vm_cfg_file);
	program_set_default_cfg_values();
	program_load_config(vm_cfg_file);
#ifdef DEBUG
	puts("Hash table:"); for(int i=0;i<KEY_ENDLIST;i++) printf("%d 0x%8.8X='%s'\n",i,cfg[i].hash,cfg[i].val);
#endif
	program_build_cmd_line(vm_name, cmd);
	puts("QEMU Command line arguments:");
	puts(cmd);
	return system(cmd); // Just Pass Qemu's Err code on
}
