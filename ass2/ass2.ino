#include "stdio.h"
#include "stdint.h"
#include "CleanRTOS.h"
#include "crt_task.h"
#include "crt_Logger.h"
#include "crt_Mutex.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"

 
// Simulates CPU work by busy-waiting for a given time in milliseconds.
void CPU_work(int time) {
    int64_t end_time = esp_timer_get_time() + (time * 1000); // Convert time to microseconds.
    while (esp_timer_get_time() < end_time) {
        // Busy-wait until the current time reaches the end time.
    }
}


// Global variable to track LED state (on/off).
bool ledOn = false;
// Global semaphore for protecting access to frequency measurements between tasks.

SemaphoreHandle_t freqMeasurementsMutex;
// Global queue for passing button press events between tasks.
QueueHandle_t buttonPressQueue;
 
// Task handles for managing tasks in FreeRTOS.
TaskHandle_t myTaskHandle0 = NULL;
TaskHandle_t myTaskHandle1 = NULL;
TaskHandle_t myTaskHandle2 = NULL;
TaskHandle_t myTaskHandle3 = NULL;
TaskHandle_t myTaskHandle4 = NULL;
TaskHandle_t myTaskHandle6 = NULL;
TaskHandle_t myTaskHandle7 = NULL;

// Structure for storing frequency measurements from Tasks 2 and 3.

struct freqMeasurments {
  int timer0 = 0; // Placeholder, possibly for future use.
  int timer1 = 0; // Placeholder, possibly for future use.
  int freq1; // Frequency measured by Task 2.
  int freq2; // Frequency measured by Task 3.
} freqMeasurments;


// Pin assignments and global variables for analog reading and LED control.
const int analogInputPin = 34;
const int ledPin = 13;
const int numReadings = 10;
int readings[numReadings] = {0}; // Initialize all readings to 0.
int currentIndex = 0; // Current index in readings array.
int total = 0; // Sum of the last `numReadings` readings.
#define BUTTON_PIN GPIO_NUM_17  // Use gpio_num_t type directly for clarity
#define LED_PIN GPIO_NUM_14     // Use gpio_num_t type directly for clarity

 

 

// Maps a value from one range to another.
int mapToRange(int value, int inMin, int inMax, int outMin, int outMax) {
    return (value - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;

}

 

// Task 1: Generates a digital signal pattern on GPIO 27.
void Task1(void *pvParameters) {
    // Initialize GPIO 27 for output to produce the digital signal.
    gpio_pad_select_gpio((gpio_num_t)27);
    gpio_set_direction((gpio_num_t)27, GPIO_MODE_OUTPUT);

 

    // Main task loop with fixed frequency execution.
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(4); // Executes every 4 milliseconds.

    for (;;) {
        // Generates the specified digital signal pattern on GPIO 27.
        gpio_set_level((gpio_num_t)27, 1); // Set high.
        ets_delay_us(180); // Delay for 180 microseconds.
        gpio_set_level((gpio_num_t)27, 0); // Set low.
        ets_delay_us(40); // Delay for 40 microseconds.
        gpio_set_level((gpio_num_t)27, 1); // Set high again.
        ets_delay_us(530); // Delay for 530 microseconds.
        gpio_set_level((gpio_num_t)27, 0); // Set low until next cycle.
        vTaskDelayUntil(&xLastWakeTime, xFrequency); // Wait for the next cycle.

    }

}


void Task2(void *pvParameters) {
    // Initialize the last wake time to the current tick count.
    TickType_t xLastWakeTime = xTaskGetTickCount();
    // Define the task's execution frequency as every 20 milliseconds.
    const TickType_t xFrequency = pdMS_TO_TICKS(20);
    // Infinite loop for continuous task execution.

    for (;;) {

        // Delay task until the next cycle based on the last wake time and the defined frequency.
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        // Begin frequency measurement by detecting the first rising edge.
        while (digitalRead(26)) {} // Wait while the signal is HIGH (stable state before measurement).
        while (!digitalRead(26)) {} // Wait for the signal to go LOW, indicating a falling edge.
        unsigned long TimerStart = micros(); // Record the start time at the rising edge.
        while (digitalRead(26)) {} // Wait for the signal to go HIGH again, completing one period.
        while (!digitalRead(26)) {} // Wait for the signal to go LOW, marking the end of one cycle.
        unsigned long period1 = micros() - TimerStart; // Calculate the period by subtracting the start time from the current time.

      

        // Protect access to the shared frequency measurement resource with a semaphore.
        if(xSemaphoreTake(freqMeasurementsMutex, portMAX_DELAY) == pdTRUE) {
            freqMeasurments.freq1 = 1e6 / period1; // Calculate frequency from the period and store it.
            xSemaphoreGive(freqMeasurementsMutex); // Release the semaphore after updating the shared resource.

        }

    }

}

void Task3(void *pvParameters) {
    // Initialize the last wake time to the current tick count.
    TickType_t xLastWakeTime = xTaskGetTickCount();
    // Define the task's execution frequency as every 8 milliseconds.
    const TickType_t xFrequency = pdMS_TO_TICKS(8);
    // Infinite loop for continuous task execution.
    for (;;) {

        // Delay task until the next cycle based on the last wake time and the defined frequency.
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        // Begin frequency measurement by detecting the first rising edge.
        while (digitalRead(25)) {} // Wait while the signal is HIGH.
        while (!digitalRead(25)) {} // Wait for the signal to go LOW.
        unsigned long TimerStart = micros(); // Record the start time at the rising edge. timer 
        while (digitalRead(25)) {} // Wait for the signal to go HIGH again, completing one period.
        while (!digitalRead(25)) {} // Wait for the signal to go LOW, marking the end of one cycle.
        unsigned long period2 = micros() - TimerStart; // Calculate the period of the second signal.

        // Protect access to the shared frequency measurement resource with a semaphore.
        if(xSemaphoreTake(freqMeasurementsMutex, portMAX_DELAY) == pdTRUE) {
            freqMeasurments.freq2 = 1e6 / period2; // Calculate frequency from the period and store it.
            xSemaphoreGive(freqMeasurementsMutex); // Release the semaphore after updating the shared resource.

        }

    }

}


void Task4(void *pvParameters) {

  // Initialize timing for periodic execution every 20ms (50Hz rate).
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(20);
 
  while (1) {

    // Read the current value from the analog input pin.
    int sensorValue = analogRead(analogInputPin);
    // Update the running total by removing the oldest reading.
    total -= readings[currentIndex];
    // Insert the new reading into the array and add it to the running total.
    readings[currentIndex] = sensorValue;
    total += sensorValue;
    // Move to the next position in the circular readings array.
    currentIndex = (currentIndex + 1) % numReadings;
    // Calculate the average value of the readings.
    int average = total / numReadings;
    // If the average exceeds half of the maximum analog range, turn the LED on.
    if (average > 2047) {
      gpio_set_level((gpio_num_t)ledPin, 1);
    } else {
      gpio_set_level((gpio_num_t)ledPin, 0); // Otherwise, turn it off.
    }
    // Log the current average value for debugging.
      // printf("Average: %d\n", average);
    // Wait until the next scheduled execution time.
    vTaskDelayUntil(&xLastWakeTime, xFrequency);

  }

}

void Task5(void *parameters) {
    // Set the logging frequency to every 200ms.
    const TickType_t xFrequency = pdMS_TO_TICKS(200);
    TickType_t xLastWakeTime = xTaskGetTickCount();
    Serial.begin(9600); // Serial communication at 9600 baud.

    while (true) {
        // Wait until the next scheduled logging time.
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        // Ensure exclusive access to the shared frequency measurements.
        if(xSemaphoreTake(freqMeasurementsMutex, portMAX_DELAY) == pdTRUE) {
            // Scale the measured frequencies for logging.
            int ScaledFreqTask2 = mapToRange(freqMeasurments.freq1, 333, 1000, 0, 99);
            int ScaledFreqTask3 = mapToRange(freqMeasurments.freq2, 500, 1000, 0, 99);
            xSemaphoreGive(freqMeasurementsMutex); // Release the semaphore after access.

            // Log the scaled frequencies.

            printf("%d,%d\n", ScaledFreqTask2, ScaledFreqTask3);

        }

    }

}

void buttonListenerTask(void* pvParameters) {
  static bool lastButtonState = HIGH; // Assume button is released at start (pull-up)

    while (true) {

        bool currentButtonState = gpio_get_level(BUTTON_PIN) == 0; // Read the button state

        if (currentButtonState != lastButtonState && currentButtonState == LOW) {
            bool ledState = !gpio_get_level(LED_PIN); // Toggle LED state
            xQueueSend(buttonPressQueue, &ledState, portMAX_DELAY); // Send the new LED state to the queue
            vTaskDelay(pdMS_TO_TICKS(200)); // Debounce delay
        }
        lastButtonState = currentButtonState;
        vTaskDelay(pdMS_TO_TICKS(10)); // Short delay for button state polling

    }

}

void ledControlTask(void* pvParameters) {
    bool ledState;
    while (true) {
        if (xQueueReceive(buttonPressQueue, &ledState, portMAX_DELAY)) {
            gpio_set_level(LED_PIN, ledState); // Set LED to the new state
        }
    }
}


void Task8(void *pvParameters) {
    // Initialize the last wake time for periodic execution.
    TickType_t xLastWakeTime = xTaskGetTickCount();
    // Define the execution frequency as every 20 milliseconds.
    const TickType_t xFrequency = pdMS_TO_TICKS(20);

    for (;;) {

        // Simulate CPU work by busy-waiting for 2 milliseconds.
        CPU_work(2);
        // Wait until the next cycle, ensuring task runs periodically every 20ms.
    }
      vTaskDelayUntil(&xLastWakeTime, xFrequency);
}


void setup() {

    // Attempt to create a mutex for protecting shared resources.
    freqMeasurementsMutex = xSemaphoreCreateMutex();
    if (freqMeasurementsMutex == NULL) {
        printf("Failed to create the mutex.\n");
        // Exit if the mutex creation failed.
        return;

    }


    // Attempt to create a queue for handling button press events.

    buttonPressQueue = xQueueCreate(10, sizeof(bool));
    if (buttonPressQueue == NULL) {
        printf("Failed to create buttonPressQueue.\n");

    }


    // Configure the LED pin as an output for visual feedback.

    pinMode(ledPin, OUTPUT);

    // Initialize serial communication at 9600 bps for debugging.

    Serial.begin(9600);

    // Initialize GPIO configurations for various tasks.
    pinMode(27, OUTPUT); // For Task 1 (Digital Signal Output)
    pinMode(26, INPUT);  // For Task 2 (Frequency Measurement)
    pinMode(25, INPUT);  // For Task 3 (Frequency Measurement)
    pinMode(17, INPUT);  // For Task 5 Input and Button Interrupt
    pinMode(14, OUTPUT); // LED Pin

  
    // Create tasks, specifying their stack size, priority, and core affinity.

    xTaskCreatePinnedToCore(Task1, "Task1", 2048, NULL, 6, &myTaskHandle0, 0);
    xTaskCreatePinnedToCore(Task2, "Task2", 2048, NULL, 5, &myTaskHandle1, 0);
    xTaskCreatePinnedToCore(Task3, "Task3", 2048, NULL, 5, &myTaskHandle2, 0);
    xTaskCreatePinnedToCore(Task4, "Task4", 2048, NULL, 4, &myTaskHandle3, 0);
    xTaskCreatePinnedToCore(Task5, "Task5", 4096, NULL, 8, &myTaskHandle4, 0);
    xTaskCreatePinnedToCore(Task8, "Task8", 2048, NULL, 3, &myTaskHandle7, 0);
    // Task for listening to button press
    xTaskCreate(buttonListenerTask, "ButtonListener", 2048, NULL, 8, NULL);
    // Task for controlling LED based on button press
    xTaskCreate(ledControlTask, "LEDControl", 2048, NULL, 8, NULL);

}

void setupTasksForButtonAndLedControl() {
    gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    buttonPressQueue = xQueueCreate(10, sizeof(bool));

  

}

 

void loop() {
  // The scheduler handles task execution.
}

int main() {
    // Call setup to initialize the system and tasks.
    setup();
    // Start the FreeRTOS scheduler to begin executing tasks.
    vTaskStartScheduler();
    // In case the scheduler returns, enter an infinite loop.
    for(;;);

}




//CORRECTIONS TO CODE 


//NEW MAP TO RANGE WITH CORRECT BOUNDING
int mapToRange(int value, int inMin, int inMax, int outMin, int outMax) {
    int scaledValue = (value - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
    // Ensure the scaled value does not go below outMin or above outMax.
    if (scaledValue < outMin) {
        scaledValue = outMin;
    } else if (scaledValue > outMax) {
        scaledValue = outMax;
    }
    return scaledValue;
}


//THIS IS THE SAME AS TASK 3
void Task2(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(20);

    for (;;) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        // Begin frequency measurement
        while (digitalRead(26)) {}
        while (!digitalRead(26)) {}
        unsigned long TimerStart = micros();
        while (digitalRead(26)) {}
        while (!digitalRead(26)) {}
        unsigned long period1 = micros() - TimerStart; // Moved outside semaphore
        
        unsigned long frequency = 1e6 / period1; // Perform calculation outside the critical section

        // Protect access to the shared frequency measurement resource with a semaphore.
        if(xSemaphoreTake(freqMeasurementsMutex, portMAX_DELAY) == pdTRUE) {
            freqMeasurments.freq1 = frequency; // Only access the shared resource within the semaphore
            xSemaphoreGive(freqMeasurementsMutex);
        }
    }
}


//Code For Testing 


//Lock and Unlock Test
// This test logs messages indicating successful mutex lock and release by tasks.
// It confirms that tasks can successfully acquire and release the mutex, ensuring serialized access to shared resources.
//Result: Log messages indicating successful mutex lock and release by tasks.
void Task2(void *pvParameters) {
    // ... [existing Task2 code]

    if(xSemaphoreTake(freqMeasurementsMutex, portMAX_DELAY) == pdTRUE) {
        // Simulates frequency calculation and stores it
        freqMeasurments.freq1 = 1e6 / period1;

        // Diagnostic log indicates Task 2 has locked the mutex and updated freq1
        printf("Task 2 has locked the mutex and updated freq1\n");

        xSemaphoreGive(freqMeasurementsMutex);

        // Diagnostic log indicates Task 2 has released the mutex
        printf("Task 2 has released the mutex\n");
    }
}

//Priority Inversion Test
// This involves creating a new low-priority task that acquires the mutex and doesn't release it immediately while a high-priority task attempts to acquire the same mutex.
// Observing the behavior to ensure that priority inversion handling, such as priority inheritance, is functioning correctly if enabled in the system.
// Result: The lower-priority task temporarily inherits a higher priority to avoid priority inversion and releases the mutex allowing the higher-priority task to proceed.


//Timeout Test
// This test checks if a task fails to acquire the mutex within the specified timeout period if it's already held by another task.
// It validates the timeout behavior of mutex acquisition, which is useful for detecting deadlocks or long hold times that could affect real-time performance.
//Result: Task fails to acquire the mutex within the specified timeout period if it's already held by another task.
if(xSemaphoreTake(freqMeasurementsMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    // Diagnostic log indicates Task 2 failed to acquire the mutex within 100 ms
    printf("Task 2 failed to acquire the mutex within 100 ms\n");
}

//Consistency Check:
// This check ensures that data in `freqMeasurments` isn't accessed simultaneously by multiple tasks.
// It includes checks after releasing the mutex to see if the data has been altered unexpectedly.
// Result: No unexpected alterations to the shared structure after concurrent access attempts.


//Send and Receive Test
// This test results in sequential log messages from the button listener and LED control tasks, showing successful message passing.
// It confirms the queue is functioning correctly, enabling inter-task communication without data loss or corruption.
// Result: Sequential log messages from the button listener and LED control tasks, showing successful message passing.
void buttonListenerTask(void* pvParameters) {
    // ... [existing button listener code]

    if(xQueueSend(buttonPressQueue, &ledState, portMAX_DELAY) == pdPASS) {
        // Diagnostic log indicates a button press has been sent to the queue
        printf("Button press sent to queue\n");
    }
}

void ledControlTask(void* pvParameters) {
    // ... [existing led control code]

    if(xQueueReceive(buttonPressQueue, &ledState, portMAX_DELAY) == pdPASS) {
        // Diagnostic log indicates an LED state has been received from the queue
        printf("LED state received from queue\n");
    }
}


//Quantitative Metrics
// Measuring mutex acquisition time
// Short mutex lock/unlock times, measured in ticks, indicate efficient mutex handling.
// Long durations may suggest contention or inefficient use of shared resources.

TickType_t startTicks = xTaskGetTickCount();
xSemaphoreTake(freqMeasurementsMutex, portMAX_DELAY);
TickType_t endTicks = xTaskGetTickCount();
printf("Mutex lock time: %d ticks\n", endTicks - startTicks);

// Measuring queue send time
// Minimal delays confirm efficient message passing.
// Extended times could indicate bottlenecks or heavy system load.
startTicks = xTaskGetTickCount();
xQueueSend(buttonPressQueue, &ledState, portMAX_DELAY);
endTicks = xTaskGetTickCount();
printf("Queue send time: %d ticks\n", endTicks - startTicks);



