#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>  // Thư viện để sử dụng time()

#define DEVICE_PATH "/dev/etx_device"
#define BUFFER_SIZE 256
//Tạo log file để ghi lại thọng tin
void log_action(const char *action) {
    FILE *log_file = fopen("device_log.txt", "a");
    if (log_file != NULL) {
        time_t current_time = time(NULL);
        fprintf(log_file, "[%s] %s\n", ctime(&current_time), action);
        fclose(log_file);
    } else {
        perror("Failed to open log file");
    }
}

int main()
{
    int fd;
    char user_input[BUFFER_SIZE];  // Mảng lưu lựa chọn người dùng
    uint8_t gpio_state;

    // Mở device driver
    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("Failed to open the device");
        return 1;
    }

    printf("Device driver opened successfully.\n");

    while (1) {
        printf("Enter your choice:\n");
        printf("0 - Turn off Loa\n");
        printf("1 - Turn on Loa\n");
        printf("2 - Check Loa state\n");
        printf("3 - Activate the sensor\n");
        printf("4 - Deactivate the sensor\n");
        printf("q - Quit\n");

        // Nhận lựa chọn từ người dùng
        if (fgets(user_input, BUFFER_SIZE, stdin) == NULL) {
            perror("Failed to read user input");
            close(fd);
            return 1;
        }

        // Xóa ký tự newline
        user_input[strcspn(user_input, "\n")] = 0;


        if (strcmp(user_input, "q") == 0 || strcmp(user_input, "Q") == 0) {
            break;
        }


        if (strcmp(user_input, "1") == 0) {

            if (write(fd, "1", 1) < 0) {
                perror("Failed to write to the device");
                close(fd);
                return 1;
            }
            printf("Loa turned ON.\n");
            log_action("Loa turned ON.");
        } else if (strcmp(user_input, "0") == 0) {
            if (write(fd, "0", 1) < 0) {
                perror("Failed to write to the device");
                close(fd);
                return 1;
            }
            printf("Loa turned OFF.\n");
            log_action("Loa turned OFF.");
        } else if (strcmp(user_input, "2") == 0) {
            if (read(fd, &gpio_state, sizeof(gpio_state)) < 0) {
                perror("Failed to read from the device");
                close(fd);
                return 1;
            }
            if (gpio_state == 1) {
                printf("Loa ON.\n");
            } else if (gpio_state == 0) {
                printf("Loa OFF.\n");
            } else {
                printf("Error: Invalid Loa state.\n");
            }
        } else if (strcmp(user_input, "3") == 0) {
            // Kích hoạt cảm biến
            if (write(fd, "3", 1) < 0) {
                perror("Failed to write to the device");
                close(fd);
                return 1;
            }
 printf("Sensor activated.\n");
            log_action("Sensor activated.");
        } else if (strcmp(user_input, "4") == 0) {
            // Tắt cảm biến
            if (write(fd, "4", 1) < 0) {
                perror("Failed to write to the device");
                close(fd);
                return 1;
            }
            printf("Sensor deactivated.\n");
            log_action("Sensor deactivated.");
        } else {
            printf("Invalid input!!! Please enter a valid option (1, 0, 2, 3, 4, q).\n"); 
        }
    }

    // Đóng device driver
    close(fd);
    printf("Device driver closed.\n");
    return 0;
}
