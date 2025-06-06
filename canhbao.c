// Fixed Linux Kernel Driver: Canh bao chay + giam sat nuoc
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>  // Thêm cho ??ng b?

// GPIO ??nh ngh?a
#define GPIO_FIRE_IN       539   // Cam bien lua
#define GPIO_WATER_IN      538   // Cam bien muc nuoc
#define GPIO_BUZZER_FIRE   534   // Coi chay (muc 0 la bat)
#define GPIO_BUZZER_WATER  537   // Coi bao het nuoc (muc 0 la bat)
#define GPIO_PUMP          540   // May bom nuoc (muc 0 la bat)

// Bien toan cuc
static dev_t dev = 0;
static struct class *dev_class;
static struct cdev etx_cdev;

unsigned int IRQ_FIRE, IRQ_WATER;
int fire_enabled = 0;
int fire_detected = 0;    // Trang thai chay hien tai
int water_available = 1;  // Trang thai nuoc (1=co, 0=het)

DEFINE_SPINLOCK(state_lock); // Bao ve trang thai chung

// ===== Ham cap nhat trang thai thiet bi =====
static void update_system_state(void) {
    if (fire_detected && fire_enabled) {
        // Co chay va sensor duoc bat
        gpio_set_value(GPIO_BUZZER_FIRE, 0); // Bat coi chay
        
        if (water_available) {
            // Co nuoc -> bat bom, tat coi het nuoc
            gpio_set_value(GPIO_PUMP, 0);
            gpio_set_value(GPIO_BUZZER_WATER, 1);
            pr_info("Chay phat hien: Bom dang chay\n");
        } else {
            // Het nuoc -> tat bom, bat coi het nuoc
            gpio_set_value(GPIO_PUMP, 1);
            gpio_set_value(GPIO_BUZZER_WATER, 0);
            pr_warn("Khong bom duoc: Het nuoc\n");
        }
    } else {
        // Khong chay hoac sensor tat
        gpio_set_value(GPIO_BUZZER_FIRE, 1); // Tat coi chay
        gpio_set_value(GPIO_PUMP, 1);        // Tat bom
        
        if (!water_available) {
            // Van bao het nuoc neu khong co nuoc
            gpio_set_value(GPIO_BUZZER_WATER, 0);
        } else {
            gpio_set_value(GPIO_BUZZER_WATER, 1);
        }
        
        if (fire_detected) {
            pr_info("Het chay: Bom & coi tat\n");
        }
    }
}

// ===== IRQ Handler cho cam bien lua =====
static irqreturn_t fire_irq_handler(int irq, void *dev_id) {
    unsigned long flags;
    int fire_state = gpio_get_value(GPIO_FIRE_IN);
    int new_fire_detected = (fire_state == 0); // 0 = co lua
    
    spin_lock_irqsave(&state_lock, flags);
    
    if (fire_detected != new_fire_detected) {
        fire_detected = new_fire_detected;
        update_system_state();
    }
    
    spin_unlock_irqrestore(&state_lock, flags);
    return IRQ_HANDLED;
}

// ===== IRQ Handler cho cam bien nuoc =====
static irqreturn_t water_irq_handler(int irq, void *dev_id) {
    unsigned long flags;
    int water_state = gpio_get_value(GPIO_WATER_IN);
    int new_water_available = (water_state == 0); // 0 = co nuoc
    
    spin_lock_irqsave(&state_lock, flags);
    
    if (water_available != new_water_available) {
        water_available = new_water_available;
        
        if (water_available) {
            pr_info("Da co nuoc tro lai\n");
        } else {
            pr_warn("Het nuoc: Bom dung, coi canh bao\n");
        }
        
        update_system_state();
    }
    
    spin_unlock_irqrestore(&state_lock, flags);
    return IRQ_HANDLED;
}

// ===== File operations =====
static ssize_t etx_write(struct file *filp, const char __user *buf, size_t len, loff_t *off) {
    char cmd;
    unsigned long flags;
    
    if (copy_from_user(&cmd, buf, 1)) return -EFAULT;

    spin_lock_irqsave(&state_lock, flags);
    
    switch (cmd) {
        case '1':
            gpio_set_value(GPIO_BUZZER_FIRE, 0);
            pr_info("Manual: Buzzer Fire ON\n");
            break;
        case '0':
            gpio_set_value(GPIO_BUZZER_FIRE, 1);
            gpio_set_value(GPIO_PUMP, 1);
            gpio_set_value(GPIO_BUZZER_WATER, 1);
            pr_info("Manual: All OFF\n");
            break;
        case '3':
            fire_enabled = 1;
            pr_info("Chay: Kich hoat cam bien lua\n");
            update_system_state(); // Cap nhat trang thai ngay
            break;
        case '4':
            fire_enabled = 0;
            pr_info("Chay: Vo hieu cam bien lua\n");
            update_system_state(); // Cap nhat trang thai ngay
            break;
        case '5':
            water_available = 1; // Reset trang thai nuoc
            gpio_set_value(GPIO_BUZZER_WATER, 1);
            pr_info("Reset canh bao het nuoc\n");
            update_system_state();
            break;
        default:
            pr_warn("Lenh khong hop le\n");
    }
    
    spin_unlock_irqrestore(&state_lock, flags);
    return len;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .write = etx_write,
};

// ===== Khoi tao driver =====
static int __init etx_driver_init(void) {
    if ((alloc_chrdev_region(&dev, 0, 1, "etx_dev")) < 0) return -1;
    cdev_init(&etx_cdev, &fops);
    if ((cdev_add(&etx_cdev, dev, 1)) < 0) goto r_del;

    if (IS_ERR(dev_class = class_create(THIS_MODULE, "etx_class"))) goto r_class;
    if (IS_ERR(device_create(dev_class, NULL, dev, NULL, "etx_device"))) goto r_device;

    // Khoi tao GPIO outputs (tat ca OFF = 1)
    gpio_request(GPIO_BUZZER_FIRE, "BUZZER_FIRE");
    gpio_direction_output(GPIO_BUZZER_FIRE, 1);

    gpio_request(GPIO_PUMP, "PUMP");
    gpio_direction_output(GPIO_PUMP, 1);

    gpio_request(GPIO_BUZZER_WATER, "BUZZER_WATER");
    gpio_direction_output(GPIO_BUZZER_WATER, 1);

    // Khoi tao GPIO inputs
    gpio_request(GPIO_FIRE_IN, "FIRE_IN");
    gpio_direction_input(GPIO_FIRE_IN);
    IRQ_FIRE = gpio_to_irq(GPIO_FIRE_IN);
    request_irq(IRQ_FIRE, fire_irq_handler, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "fire_irq", NULL);

    gpio_request(GPIO_WATER_IN, "WATER_IN");
    gpio_direction_input(GPIO_WATER_IN);
    IRQ_WATER = gpio_to_irq(GPIO_WATER_IN);
    request_irq(IRQ_WATER, water_irq_handler, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "water_irq", NULL);

    // Doc trang thai ban dau
    fire_detected = (gpio_get_value(GPIO_FIRE_IN) == 0);
    water_available = (gpio_get_value(GPIO_WATER_IN) == 0);
    
    pr_info("Driver chay thanh cong\n");
    pr_info("Trang thai ban dau - Fire: %d, Water: %d\n", fire_detected, water_available);
    
    return 0;

r_device:
    class_destroy(dev_class);
r_class:
    cdev_del(&etx_cdev);
r_del:
    unregister_chrdev_region(dev, 1);
    return -1;
}

// ===== Thoat driver =====
static void __exit etx_driver_exit(void) {
    free_irq(IRQ_FIRE, NULL);
    free_irq(IRQ_WATER, NULL);

    gpio_free(GPIO_FIRE_IN);
    gpio_free(GPIO_WATER_IN);
    gpio_free(GPIO_BUZZER_FIRE);
    gpio_free(GPIO_BUZZER_WATER);
    gpio_free(GPIO_PUMP);

    device_destroy(dev_class, dev);
    class_destroy(dev_class);
    cdev_del(&etx_cdev);
    unregister_chrdev_region(dev, 1);
    pr_info("Driver da go bo\n");
}

module_init(etx_driver_init);
module_exit(etx_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("NHOM 4");
MODULE_DESCRIPTION("Driver canh bao chay + giam sat nuoc");
MODULE_VERSION("2.3");
