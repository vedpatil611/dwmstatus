/*
 * Copy me if you can.
 * by 20h
 */

#define _BSD_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <X11/Xlib.h>

#define LOW_BATTERY_WARNING_THRESHOLD 20

char *tzutc = "UTC";
char *tz = "Asia/Kolkata";

static Display *dpy;

char *
smprintf(char *fmt, ...)
{
	va_list fmtargs;
	char *ret;
	int len;

	va_start(fmtargs, fmt);
	len = vsnprintf(NULL, 0, fmt, fmtargs);
	va_end(fmtargs);

	ret = malloc(++len);
	if (ret == NULL) {
		perror("malloc");
		exit(1);
	}

	va_start(fmtargs, fmt);
	vsnprintf(ret, len, fmt, fmtargs);
	va_end(fmtargs);

	return ret;
}

void
settz(char *tzname)
{
	setenv("TZ", tzname, 1);
}

char *
mktimes(char *fmt, char *tzname)
{
	char buf[129];
	time_t tim;
	struct tm *timtm;

	settz(tzname);
	tim = time(NULL);
	timtm = localtime(&tim);
	if (timtm == NULL)
		return smprintf("");

	if (!strftime(buf, sizeof(buf)-1, fmt, timtm)) {
		fprintf(stderr, "strftime == 0\n");
		return smprintf("");
	}

	return smprintf("%s", buf);
}

void
setstatus(char *str)
{
	XStoreName(dpy, DefaultRootWindow(dpy), str);
	XSync(dpy, False);
}

char *
loadavg(void)
{
	double avgs[3];

	if (getloadavg(avgs, 3) < 0)
		return smprintf("");

	return smprintf("%.2f %.2f %.2f", avgs[0], avgs[1], avgs[2]);
}

char *
readfile(char *base, char *file)
{
	char *path, line[513];
	FILE *fd;

	memset(line, 0, sizeof(line));

	path = smprintf("%s/%s", base, file);
	fd = fopen(path, "r");
	free(path);
	if (fd == NULL)
		return NULL;

	if (fgets(line, sizeof(line)-1, fd) == NULL)
		return NULL;
	fclose(fd);

	return smprintf("%s", line);
}

char *
getbattery(char *base)
{
	char *co, status;
	int descap, remcap;

	descap = -1;
	remcap = -1;

	co = readfile(base, "present");
	if (co == NULL)
		return smprintf("");
	if (co[0] != '1') {
		free(co);
		return smprintf("not present");
	}
	free(co);

	co = readfile(base, "charge_full_design");
	if (co == NULL) {
		co = readfile(base, "energy_full_design");
		if (co == NULL)
			return smprintf("");
	}
	sscanf(co, "%d", &descap);
	free(co);

	co = readfile(base, "charge_now");
	if (co == NULL) {
		co = readfile(base, "energy_now");
		if (co == NULL)
			return smprintf("");
	}
	sscanf(co, "%d", &remcap);
	free(co);

	co = readfile(base, "status");
	if (!strncmp(co, "Discharging", 11)) {
		status = '-';
	} else if(!strncmp(co, "Charging", 8)) {
		status = '+';
	} else {
		status = '?';
	}

	if (remcap < 0 || descap < 0)
		return smprintf("invalid");

	return smprintf("%.0f%%%c", ((float)remcap / (float)descap) * 100, status);
}

char* getBattery(char* base)
{
	char* co;
	char status;
	int cap;
	static u_int8_t flags = 0;

	co = readfile(base, "capacity");
	sscanf(co ,"%d", &cap);
	free(co);

	co = readfile(base, "status");
	if (!strncmp(co, "Discharging", 11)) {
		status = '-';
		if(cap <= LOW_BATTERY_WARNING_THRESHOLD && (flags & 1) == 0)
		{
			flags |= 1;
			char* lowBatterySyntax = "notify-send -u critical \"Low Battery\"";
			system(lowBatterySyntax);
		}
	} else if(!strncmp(co, "Charging", 8)) {
		status = '+';
		if(flags & 1 && cap > LOW_BATTERY_WARNING_THRESHOLD)
		{
			flags = 0;
		}
	} else {
		status = '?';
	}
	free(co);

	return smprintf("%d%%%c", cap, status);
}

char *
gettemperature(char *base, char *sensor)
{
	char *co;

	co = readfile(base, sensor);
	if (co == NULL)
		return smprintf("");
	return smprintf("%02.0f°C", atof(co) / 1000);
}

char* getMemory() 
{
	FILE* fp = popen("free -h | grep Mem", "r");

	char temp[8];
	char used[8];
	fscanf(fp, "%s %s %s", temp, temp, used);

	pclose(fp);

	return smprintf("%s", used);
}

char* getVolume()
{
	FILE* fp = popen("pactl get-sink-volume 0", "r");
	char temp[16];
	char volume[8];
	fscanf(fp, "%s %s %s %s %s", temp, temp, temp, temp, volume);
	pclose(fp);
	return smprintf("%s", volume);
}

char* getDiskSpace()
{
	FILE* fp = popen("df -H | grep sdb3", "r");
	char temp[16];
	char avail[8];
	fscanf(fp, "%s %s %s %s", temp, temp, temp, avail);
	pclose(fp);
	return smprintf("%s", avail);
}

int main(void)
{
	char *status;
	char *bat1;
	char *tmbln;
	char *mem;
	char *volume;
	char *disk;

	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "dwmstatus: cannot open display.\n");
		return 1;
	}

	for (;;sleep(1)) {
		bat1 = getBattery("/sys/class/power_supply/BAT1");
		tmbln = mktimes("%a %d %b %H:%M %Y", tz);
		mem = getMemory();
		volume = getVolume();
		disk = getDiskSpace();

		status = smprintf("V: %s | M:%s | D:%s | B:%s | T:%s", volume, mem, disk, bat1, tmbln);
		setstatus(status);

		free(bat1);
		free(tmbln);
		free(mem);
		free(volume);
		free(disk);
		free(status);
	}

	XCloseDisplay(dpy);

	return 0;
}

