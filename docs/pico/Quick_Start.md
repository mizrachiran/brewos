# Quick Start Guide - Pico Firmware Development

This guide helps you get started with Pico firmware development.

**See also:** [Feature Status Table](Feature_Status_Table.md) for current implementation status.

## Recommended First Steps

### Step 1: Create Hardware Abstraction Layer (2-3 hours)

This allows you to develop and test without physical hardware.

**Create these files:**
- `src/pico/include/hardware.h`
- `src/pico/src/hardware.c`

**Key functions to implement:**
```c
// ADC reading
uint16_t hw_read_adc(uint8_t channel);
float hw_adc_to_voltage(uint16_t adc_value);

// SPI for MAX31855
bool hw_spi_read_max31855(uint32_t* data);
float hw_max31855_to_temp(uint32_t data);

// PWM output
void hw_set_pwm_duty(uint8_t channel, float duty_percent);

// GPIO
void hw_set_gpio(uint8_t pin, bool state);
bool hw_read_gpio(uint8_t pin);
```

**Simulation mode:**
- Add `#define HW_SIMULATION_MODE 1` for development
- Return simulated values
- Switch to `0` when ready for hardware

### Step 2: Implement First Sensor - Brew NTC (3-4 hours)

**Why start here:**
- Simplest sensor (just ADC)
- Most critical for control
- Good learning exercise

**Steps:**
1. Initialize ADC in `hardware.c`
2. Read ADC channel for brew NTC
3. Convert to voltage
4. Convert to temperature (Steinhart-Hart)
5. Add filtering (moving average)
6. Test in simulation first

**Test:**
- Verify ADC reading
- Verify temperature calculation
- Verify filtering

### Step 3: Connect Sensor to Control Loop (1-2 hours)

**Steps:**
1. Replace simulation in `sensors.c` with real reading
2. Verify temperature appears in status
3. Check ESP32 receives correct values
4. Monitor for stability

### Step 4: Implement Safety Interlock (2-3 hours)

**Start with water level:**
1. Read water level sensor
2. Add check in `safety.c`
3. Test safe state entry
4. Test recovery

**Then add:**
- Over-temperature protection
- Sensor fault detection

## Development Workflow

### Daily Workflow

1. **Start with simulation**
   - Test logic changes in simulation
   - Verify behavior is correct
   - Check timing and performance

2. **Test on hardware**
   - Switch to hardware mode
   - Test one component at a time
   - Compare with simulation

3. **Iterate**
   - Fix issues
   - Improve accuracy
   - Add error handling

### Testing Checklist

Before moving to next sensor:
- [ ] Reading is stable (no excessive noise)
- [ ] Values are in expected range
- [ ] Filtering works correctly
- [ ] Error handling works (disconnect sensor)
- [ ] Integration with control loop works

## Common Issues & Solutions

### Issue: ADC reading unstable
**Solution:**
- Add more filtering (increase sample count)
- Check wiring/grounding
- Verify reference voltage

### Issue: Temperature calculation wrong
**Solution:**
- Verify Steinhart-Hart constants
- Check calibration values
- Verify ADC voltage calculation

### Issue: Control loop timing issues
**Solution:**
- Monitor loop execution time
- Use hardware timers
- Optimize sensor reading

## Next Priorities

After hardware abstraction:

1. **Complete all sensors** (Week 2)
   - Steam NTC
   - MAX31855 thermocouple
   - Pressure sensor
   - Digital inputs

2. **Complete safety system** (Week 3)
   - All interlocks
   - Safe state
   - Watchdog

3. **Complete control** (Week 4)
   - Hardware outputs
   - PID tuning
   - Heating strategies

## Getting Help

- Check [Feature_Status_Table.md](Feature_Status_Table.md) for current implementation status
- See [Implementation_Plan.md](Implementation_Plan.md) for historical phase summaries
- Review [Architecture.md](Architecture.md) for system design
- See [Requirements.md](Requirements.md) for specifications
- Check code comments for implementation details

## Tips

1. **Use simulation liberally** - Test logic before hardware
2. **One thing at a time** - Don't try to do everything at once
3. **Test thoroughly** - Especially safety-critical code
4. **Document as you go** - Write comments while coding
5. **Commit often** - Small, focused commits

---

**Ready to start?** Begin with Step 1: Create Hardware Abstraction Layer

