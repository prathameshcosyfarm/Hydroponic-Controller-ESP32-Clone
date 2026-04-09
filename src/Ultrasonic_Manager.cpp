#include <Arduino.h>
#include <algorithm>
#include <vector>

// Configuration constants
#define TANK_HEIGHT_CM 30.0
#define TANK_MOTOR_SUCTION_CM 12.0f
#define TANK_SAFETY_BUFFER_CM 2.0f
#define SENSOR_DEAD_ZONE_CM 5.0
#define TRIG_PIN 10
#define ECHO_PIN 11

/**
 * Performs a raw distance measurement.
 */
float getRawDistance()
{
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    // Optimized timeout: 5000us covers ~85cm.
    long duration = pulseIn(ECHO_PIN, HIGH, 5000);
    if (duration == 0)
        return -1; // Timeout or no echo

    // Calculate distance using standard 20C speed of sound if no temp available
    float speedOfSound = 0.0343f;
    return (duration * speedOfSound) / 2.0f;
}

/**
 * Uses a Median Filter to get a stable distance reading.
 * This helps ignore "random" spikes caused by the blind zone or reflections.
 */
float getFilteredDistance(int samples = 5)
{
    std::vector<float> readings;
    for (int i = 0; i < samples; i++)
    {
        float d = getRawDistance();
        if (d >= 2.0f && d <= 22.0f)
            readings.push_back(d);
        delay(60);
    }

    if (readings.size() < 3)
        return -1;

    std::sort(readings.begin(), readings.end());
    return readings[readings.size() / 2];
}

/**
 * Calculates tank percentage with dead-zone handling.
 */
int calculateTankPercentage()
{
    static float smoothedDistance = -1.0f;
    const float TANK_EMA_SLOW = 0.15f;
    const float TANK_EMA_FAST = 0.80f;
    const float TANK_JUMP_THRESHOLD = 1.5f;

    float distance = getFilteredDistance(11);

    // Error handling
    if (distance < 0)
        return -1;

    // Logic: If distance is less than the dead zone, the tank is likely full.
    // Modified AJ-SR04M is now reliable down to ~5cm.
    if (distance < SENSOR_DEAD_ZONE_CM)
    {
        Serial.println("Warning: Water in Blind Zone. Assuming 100% full.");
        return 100;
    }

    // --- Moving Average (EMA) Stage ---
    if (smoothedDistance < 0)
    {
        smoothedDistance = distance; // First valid reading
    }
    else
    {
        // Adaptive EMA: Check if distance change exceeds threshold
        float diff = abs(distance - smoothedDistance);
        float currentAlpha = (diff > TANK_JUMP_THRESHOLD) ? TANK_EMA_FAST : TANK_EMA_SLOW;
        smoothedDistance = (currentAlpha * distance) + (1.0f - currentAlpha) * smoothedDistance;
    }

    distance = smoothedDistance;

    // Calculate water depth
    float waterDepth = TANK_HEIGHT_CM - distance;

    // Constrain depth to physical limits
    if (waterDepth < 0)
        waterDepth = 0;
    if (waterDepth > TANK_HEIGHT_CM)
        waterDepth = TANK_HEIGHT_CM;

    int percentage = (int)((waterDepth / TANK_HEIGHT_CM) * 100.0);

    Serial.printf("Distance: %.2f cm | Water Depth: %.2f cm | Fill: %d%%\n",
                  distance, waterDepth, percentage);

    return percentage;
}