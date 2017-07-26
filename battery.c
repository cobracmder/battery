/*
 * Copyright (c) 2017 Carlos Cardenas <cardenas12@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/sensors.h>
#include <sys/sysctl.h>

#include <err.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char* acpi_pow = "acpiac";
static const char* acpi_bat = "acpibat";
static const char* bat_volt = "voltage";
static const char* bat_cvolt = "current voltage";
static const char* bat_rate = "rate";
static const char* bat_last = "last full capacity";
static const char* bat_warn = "warning capacity";
static const char* bat_low = "low capacity";
static const char* bat_rem = "remaining capacity";
static const char* bat_cap = "design capacity";
static const enum sensor_type bat_types[] = {SENSOR_VOLTS_DC, SENSOR_WATTS,
                                      SENSOR_WATTHOUR, SENSOR_INTEGER};

SIMPLEQ_HEAD(, ac_stat) ac_head;
struct ac_stat {
    SIMPLEQ_ENTRY(ac_stat) entries;
    int online;
};

SIMPLEQ_HEAD(, bat_stat) bat_head;
struct bat_stat {
    SIMPLEQ_ENTRY(bat_stat) entries;
    char raw_bat[32];
    int64_t voltage;
    int64_t cur_voltage;
    int64_t rate;
    int64_t last_cap;
    int64_t warn_cap;
    int64_t low_cap;
    int64_t rem_cap;
    int64_t cap;
};

#ifdef DEBUG
static void print_sensordev(struct sensordev *p, int sublevel) {
    enum sensor_type type;

    printf("Num[%d] Name[%s] Count[%d]\n", p->num, p->xname, p->sensors_count);
    if (sublevel) {
        for (type = 0; type < SENSOR_MAX_TYPES; type++) {
            printf("Type[%d] -> Num[%d]\n", type, p->maxnumt[type]);
        }
    }
}

static void print_sensor(struct sensor *p) {
    if (p->type == SENSOR_VOLTS_DC || p->type == SENSOR_WATTS ||
            p->type == SENSOR_WATTHOUR) {
        float value = p->value / 1000000.0;
        printf("Status[%d] Type[%s] Num[%d] Desc[%s] Flags[%d] Value[%.2f]\n",
               p->status, sensor_type_s[p->type], p->numt, p->desc,
               p->flags, value);
    } else {
        printf("Status[%d] Type[%s] Num[%d] Desc[%s] Flags[%d] Value[%ld]\n",
               p->status, sensor_type_s[p->type], p->numt, p->desc,
               p->flags, p->value);
    }
}
#endif

static void process_bat_sensor(struct bat_stat *stat, struct sensor *sensor) {
    switch(sensor->type) {
    case SENSOR_VOLTS_DC:
        if(strcmp(sensor->desc, bat_volt) == 0)
            stat->voltage = sensor->value;
        if(strcmp(sensor->desc, bat_cvolt) == 0)
            stat->cur_voltage = sensor->value;
        break;
    case SENSOR_WATTS:
        if(strcmp(sensor->desc, bat_rate) == 0)
            stat->rate = sensor->value;
        break;
    case SENSOR_WATTHOUR:
        if(strcmp(sensor->desc, bat_last) == 0)
            stat->last_cap = sensor->value;
        if(strcmp(sensor->desc, bat_warn) == 0)
            stat->warn_cap = sensor->value;
        if(strcmp(sensor->desc, bat_low) == 0)
            stat->low_cap = sensor->value;
        if(strcmp(sensor->desc, bat_rem) == 0)
            stat->rem_cap = sensor->value;
        if(strcmp(sensor->desc, bat_cap) == 0)
            stat->cap = sensor->value;
        break;
    case SENSOR_INTEGER:
        strlcpy(stat->raw_bat, sensor->desc, sizeof(stat->raw_bat));
        break;
    default:
        break;
    }
}

static void print_stat(bool print_short) {
    struct ac_stat *ac;
    struct bat_stat *bat;
    int c_ac = 0;
    int c_bat = 0;
    float rem_per;

    SIMPLEQ_FOREACH(ac, &ac_head, entries) {
        if (print_short) {
            printf("AC%d: %s ",c_ac, ac->online? "ON" : "OFF");
        } else {
            printf("AC%d: %s\n",c_ac, 
                    ac->online? "Connected" : "Disconnected");
        }
        c_ac++;
    }

    SIMPLEQ_FOREACH(bat, &bat_head, entries) {
        rem_per = (float) bat->rem_cap / bat->cap;
        rem_per *= 100.0;
        if (print_short) {
            printf("BAT%d: %.2f%% (%s) ", c_bat, rem_per, bat->raw_bat);
        } else {
            printf("Battery%d: %s\n", c_bat, bat->raw_bat);
            printf("Battery%d: %.2f%% remaining\n", 
                    c_bat, rem_per);
            printf("Battery%d: design capacity of %.2f Wh\n", 
                    c_bat, bat->cap / 1000000.0);
            printf("Battery%d: rate of %.2f W\n", 
                    c_bat, bat->rate / 1000000.0);
            printf("Battery%d: Voltage is %.2f VDC out of %.2f VDC\n", 
                    c_bat, bat->cur_voltage / 1000000.0, 
                    bat->voltage / 1000000.0);
            printf("Battery%d: last charged to %.2f Wh\n", 
                    c_bat, bat->last_cap / 1000000.0);
            printf("Battery%d: low capacity is set for %.2f Wh\n", 
                    c_bat, bat->low_cap / 1000000.0);
            printf("Battery%d: warning capacity is set for %.2f Wh\n", 
                    c_bat, bat->warn_cap / 1000000.0);
            printf("Battery%d: remaining capacity at %.2f Wh\n", 
                    c_bat, bat->rem_cap / 1000000.0);
        }
        c_bat++;
    }
    printf("\n");
}

static void usage() {
    printf("usage: %s [OPTIONS]\n", getprogname());
    printf("Simple utility for printing battery status\n");
    printf("\t-h\t\tthis help message\n");
    printf("\t-v\t\tverbose\n");
}

int main(int argc, char **argv) {
    struct ac_stat *ac;
    struct bat_stat *bat;
    struct sensor sensor;
    struct sensordev sensordev;

    int mib[] = { CTL_HW, HW_SENSORS, 0, 0, 0 };
    size_t slen, sdlen, aclen, batlen;
    int first_ac, first_bat, sd_num;

    bool print_short = true;
    int opt_ch;

    first_ac = 1;
    first_bat = 1;
    mib[0] = CTL_HW;
    mib[1] = HW_SENSORS;

    SIMPLEQ_INIT(&ac_head);
    SIMPLEQ_INIT(&bat_head);

    slen = sizeof(struct sensor);
    sdlen = sizeof(struct sensordev);
    aclen = sizeof(struct ac_stat);
    batlen = sizeof(struct bat_stat);

    /* parse opts */
    while ((opt_ch = getopt(argc, argv, "hv")) != -1) {
        switch(opt_ch) {
        case 'v':
            print_short = false;
            break;
        default:
            usage();
            return 1;
        }
    }
    argc -= optind;
    argv += optind;

    for (sd_num = 0; ;sd_num++) {
        mib[2] = sd_num;
        if (sysctl(mib, 3, &sensordev, &sdlen, NULL, 0) == -1) {
            if (errno == ENXIO)
                continue;
            if (errno == ENOENT)
                break;
        }
        /* check if AC */
        if (strncmp(acpi_pow, sensordev.xname, strlen(acpi_pow)) == 0) {
            mib[3] = SENSOR_INDICATOR;
            mib[4] = 0;
#ifdef DEBUG
            print_sensordev(&sensordev, 0);
#endif
            if (sysctl(mib, 5, &sensor, &slen, NULL, 0) == -1) {
                if (errno == ENXIO)
                    continue;
                if (errno == ENOENT)
                    break;
            }
#ifdef DEBUG
            print_sensor(&sensor);
#endif
            // populate ac_stat
            ac = (struct ac_stat*)malloc(aclen);
            if (ac != NULL) {
                ac->online = sensor.value;
                if (first_ac) {
                    SIMPLEQ_INSERT_HEAD(&ac_head, ac, entries);
                    first_ac = 0;
                } else {
                    SIMPLEQ_INSERT_TAIL(&ac_head, ac, entries);
                }
            }
        }
        /* check if Battery */
        if (strncmp(acpi_bat, sensordev.xname, strlen(acpi_bat)) == 0) {
            int s_type, n;
#ifdef DEBUG
            print_sensordev(&sensordev, 0);
#endif
            bat = (struct bat_stat*)malloc(batlen);
            /* iterate sensor types for bat */
            for (s_type = 0; s_type < 4; s_type++) {
                mib[3] = bat_types[s_type];
                for (n = 0; n < sensordev.sensors_count; n++) {
                    mib[4] = n;
                    if (sysctl(mib, 5, &sensor, &slen, NULL, 0) == -1) {
                        if (errno == ENXIO)
                            continue;
                        if (errno == ENOENT)
                            break;
                    }
#ifdef DEBUG
                    print_sensor(&sensor);
#endif
                    if (bat != NULL) {
                        process_bat_sensor(bat, &sensor);
                    }
                }
            }
            // insert bat_stat
            if (bat != NULL) {
                if (first_bat) {
                    SIMPLEQ_INSERT_HEAD(&bat_head, bat, entries);
                    first_bat = 0;
                } else {
                    SIMPLEQ_INSERT_TAIL(&bat_head, bat, entries);
                }
            }
        }
    }
    print_stat(print_short);
    return 0;
}
