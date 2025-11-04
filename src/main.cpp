/*
 * SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include <Arduino.h>
#include "M5Unified.h"
#include "main.h"
#include <esp_task_wdt.h>
#include <esp_now.h>
#include <WiFi.h>
#include <SCServo.h>
/* Display flushing */

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    int w = (area->x2 - area->x1 + 1);
    int h = (area->y2 - area->y1 + 1);

    M5.Display.startWrite();                            /* Start new TFT transaction */
    M5.Display.setAddrWindow(area->x1, area->y1, w, h); /* set the working window */

    M5.Display.writePixels((lgfx::rgb565_t *)&color_p->full, w * h);

    M5.Display.endWrite();     /* terminate TFT transaction */
    lv_disp_flush_ready(disp); /* tell lvgl that flushing is done */
    USBSerial.println("my_disp_flush");
}
/*Read the touchpad*/
void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{
    uint16_t touchX, touchY;
    bool touched;

    touched = M5.Display.getTouch(&touchX, &touchY);

    if (!touched) {
        data->state = LV_INDEV_STATE_REL;
        USBSerial.println("my_touchpad_read 1");
    } else {
        data->state = LV_INDEV_STATE_PR;
        USBSerial.println("my_touchpad_read 2");

        data->point.x = touchX;
        data->point.y = touchY;
    }
    USBSerial.println("my_touchpad_read");
}
void lv_rt_loga(const char *buf)
{
    USBSerial.println(buf);
}

class Lvgl_tasks {
private:
    TaskHandle_t task_lvgl_handle;
    QueueHandle_t Lvgl_Queue;

public:
    struct Lvgl_msg {
        uint8_t cmd;
        union {
            struct {
                uint16_t x;
                uint16_t y;
            } posie;
            uint8_t rgb[4];
        };
    };
    Lvgl_tasks(/* args */)
    {
        task_lvgl_handle = NULL;
        Lvgl_Queue       = xQueueCreate(10, sizeof(struct Lvgl_msg));
    }
    void Lvgl_rpc(struct Lvgl_msg &msg)
    {
        xQueueSend(Lvgl_Queue, &msg, 0);
    }
    static void example_lvgl_port_task(void *arg)
    {
        Lvgl_tasks *self = (Lvgl_tasks *)arg;
        struct Lvgl_msg msg;
        for (;;) {
            if (xQueueReceive(self->Lvgl_Queue, &msg, 0)) {
                // todo:
            }
            lvgl_loop();
        }
    }
    void Lvgl_tasks_init()
    {
        lvgl_setup();
        xTaskCreate(example_lvgl_port_task, "LVGL", 8 * 1024, this, 2, &task_lvgl_handle);
    }
    ~Lvgl_tasks()
    {
    }
};

#define S_RXD       (14)   // RXD_PIN
#define S_TXD       (6)    // TXD_PIN
#define PWR_EN      (13)   // PWR_EN_PIN
#define SERVO_SPEED (800)  // 舵机转动速度

class Slave_tasks {
private:
    QueueHandle_t Slave_Queue;
    uint8_t x_min_angle[2];
    uint8_t x_max_angle[2];
    uint8_t y_min_angle[2];
    uint8_t y_max_angle[2];
    SCSCL sc;

public:
    int mappedX;
    int mappedY;
    struct Slave_msg {
        uint8_t cmd;
        union {
            struct {
                uint16_t x;
                uint16_t y;
            } posie;
            uint8_t rgb[4];
        };
    };

    Slave_tasks(/* args */)
    {
        Slave_Queue = xQueueCreate(10, sizeof(struct Slave_msg));
        pinMode(PWR_EN, OUTPUT);
        digitalWrite(PWR_EN, HIGH);
        Serial1.begin(1000000, SERIAL_8N1, S_RXD, S_TXD);
        sc.pSerial = &Serial1;
        for (int i = 0; i < 2; i++) {
            x_min_angle[i] = sc.readByte(1, SCSCL_MIN_ANGLE_LIMIT_L + i);
            x_max_angle[i] = sc.readByte(1, SCSCL_MAX_ANGLE_LIMIT_L + i);
        }
    }
    void push(struct Slave_msg &msg)
    {
        xQueueSend(Slave_Queue, &msg, 0);
    }
    void update()
    {
        struct Slave_msg msg;
        if (xQueueReceive(Slave_Queue, &msg, 0)) {
            // todo:
            sc.WritePos(1, mappedX, 0, SERVO_SPEED);
            sc.WritePos(2, mappedY, 0, SERVO_SPEED);
        }
    }
    ~Slave_tasks()
    {
    }
};

Slave_tasks Slave_task;
Lvgl_tasks Lvgl_task;
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
{
    Slave_tasks::Slave_msg myData;
    memcpy(&myData, incomingData, sizeof(myData));
    Slave_task.push(myData);
}

void setup()
{
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Power.begin();                // Init power  初始化电源模块
    M5.Power.setChargeCurrent(600);  // 设置充电电流为600mA
    USBSerial.begin(115200);

    // 屏幕设置
    M5.Display.setRotation(1);  // 默认横向显示
    M5.Display.setColorDepth(16);
    M5.Display.setBrightness(70);

    Lvgl_task.Lvgl_tasks_init();

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    esp_now_init();
    esp_now_register_recv_cb(OnDataRecv);
}

void loop()
{
    // esp_task_wdt_reset();
    Slave_task.update();
}