#pragma once
struct device { int x; };
#define DEVICE_DT_GET(x) ((const struct device *)0)
#define DT_CHOSEN(x) 0
#define DT_PATH(x) 0
#define device_init(d) 0
#define device_is_ready(d) 1
