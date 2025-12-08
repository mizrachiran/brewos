/**
 * BrewOS Card - Custom Home Assistant Lovelace Card
 * 
 * A beautiful card for displaying and controlling your BrewOS espresso machine.
 * 
 * Installation:
 * 1. Copy this file to /config/www/brewos-card.js
 * 2. Add to Lovelace resources:
 *    url: /local/brewos-card.js
 *    type: module
 * 3. Add card to your dashboard
 */

const CARD_VERSION = '1.0.0';

const styles = `
  :host {
    --brewos-primary: #d97706;
    --brewos-accent: #f59e0b;
    --brewos-success: #10b981;
    --brewos-danger: #ef4444;
    --brewos-warning: #f59e0b;
    --brewos-info: #3b82f6;
    --brewos-muted: #6b7280;
    --brewos-bg: var(--ha-card-background, var(--card-background-color, #fff));
    --brewos-text: var(--primary-text-color, #1f2937);
    --brewos-text-secondary: var(--secondary-text-color, #6b7280);
  }
  
  .brewos-card {
    padding: 16px;
    font-family: var(--paper-font-body1_-_font-family);
  }
  
  .header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    margin-bottom: 16px;
  }
  
  .header-left {
    display: flex;
    align-items: center;
    gap: 12px;
  }
  
  .machine-icon {
    width: 48px;
    height: 48px;
    border-radius: 12px;
    background: linear-gradient(135deg, var(--brewos-primary) 0%, var(--brewos-accent) 100%);
    display: flex;
    align-items: center;
    justify-content: center;
    color: white;
    font-size: 24px;
  }
  
  .machine-info h2 {
    margin: 0;
    font-size: 18px;
    font-weight: 600;
    color: var(--brewos-text);
  }
  
  .machine-info .status {
    display: flex;
    align-items: center;
    gap: 6px;
    margin-top: 2px;
  }
  
  .status-indicator {
    width: 8px;
    height: 8px;
    border-radius: 50%;
  }
  
  .status-indicator.standby { background: var(--brewos-muted); }
  .status-indicator.heating { background: var(--brewos-warning); animation: pulse 1.5s infinite; }
  .status-indicator.ready { background: var(--brewos-success); }
  .status-indicator.brewing { background: var(--brewos-info); animation: pulse 0.8s infinite; }
  .status-indicator.fault { background: var(--brewos-danger); }
  
  @keyframes pulse {
    0%, 100% { opacity: 1; }
    50% { opacity: 0.5; }
  }
  
  .status-text {
    font-size: 13px;
    color: var(--brewos-text-secondary);
    text-transform: capitalize;
  }
  
  .power-toggle {
    padding: 8px 16px;
    border-radius: 20px;
    border: none;
    cursor: pointer;
    font-size: 14px;
    font-weight: 500;
    display: flex;
    align-items: center;
    gap: 6px;
    transition: all 0.2s;
  }
  
  .power-toggle.off {
    background: var(--brewos-muted);
    color: white;
  }
  
  .power-toggle.on {
    background: var(--brewos-success);
    color: white;
  }
  
  .power-toggle:hover {
    transform: scale(1.05);
    filter: brightness(1.1);
  }
  
  /* Temperature Section */
  .temps-section {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 12px;
    margin-bottom: 16px;
  }
  
  .temp-card {
    background: var(--secondary-background-color, #f3f4f6);
    border-radius: 12px;
    padding: 16px;
    position: relative;
    overflow: hidden;
  }
  
  .temp-card.brew { border-left: 3px solid var(--brewos-primary); }
  .temp-card.steam { border-left: 3px solid var(--brewos-info); }
  
  .temp-label {
    font-size: 12px;
    text-transform: uppercase;
    letter-spacing: 0.5px;
    color: var(--brewos-text-secondary);
    margin-bottom: 4px;
  }
  
  .temp-value {
    font-size: 28px;
    font-weight: 700;
    font-family: 'SF Mono', 'Monaco', monospace;
    color: var(--brewos-text);
  }
  
  .temp-value .unit {
    font-size: 16px;
    font-weight: 400;
    color: var(--brewos-text-secondary);
  }
  
  .temp-setpoint {
    font-size: 12px;
    color: var(--brewos-text-secondary);
    margin-top: 4px;
  }
  
  .temp-progress {
    position: absolute;
    bottom: 0;
    left: 0;
    height: 3px;
    background: linear-gradient(90deg, var(--brewos-primary), var(--brewos-accent));
    transition: width 0.5s ease;
  }
  
  /* Pressure & Shot Section */
  .metrics-row {
    display: grid;
    grid-template-columns: repeat(3, 1fr);
    gap: 12px;
    margin-bottom: 16px;
  }
  
  .metric-card {
    background: var(--secondary-background-color, #f3f4f6);
    border-radius: 12px;
    padding: 12px;
    text-align: center;
  }
  
  .metric-icon {
    font-size: 20px;
    margin-bottom: 4px;
  }
  
  .metric-value {
    font-size: 20px;
    font-weight: 600;
    font-family: 'SF Mono', 'Monaco', monospace;
    color: var(--brewos-text);
  }
  
  .metric-label {
    font-size: 11px;
    color: var(--brewos-text-secondary);
    text-transform: uppercase;
  }
  
  /* Shot Timer (when brewing) */
  .shot-timer {
    background: linear-gradient(135deg, #1e3a5f 0%, #0c1929 100%);
    border-radius: 16px;
    padding: 24px;
    text-align: center;
    margin-bottom: 16px;
    color: white;
  }
  
  .shot-timer .timer {
    font-size: 56px;
    font-weight: 700;
    font-family: 'SF Mono', 'Monaco', monospace;
    margin-bottom: 8px;
  }
  
  .shot-timer .shot-stats {
    display: flex;
    justify-content: center;
    gap: 32px;
  }
  
  .shot-stat {
    text-align: center;
  }
  
  .shot-stat .value {
    font-size: 24px;
    font-weight: 600;
  }
  
  .shot-stat .label {
    font-size: 11px;
    opacity: 0.8;
    text-transform: uppercase;
  }
  
  /* Actions */
  .actions {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 12px;
  }
  
  .action-btn {
    padding: 12px;
    border-radius: 12px;
    border: none;
    cursor: pointer;
    font-size: 14px;
    font-weight: 500;
    display: flex;
    align-items: center;
    justify-content: center;
    gap: 8px;
    transition: all 0.2s;
    background: var(--secondary-background-color, #f3f4f6);
    color: var(--brewos-text);
  }
  
  .action-btn:hover {
    filter: brightness(0.95);
  }
  
  .action-btn.primary {
    background: var(--brewos-primary);
    color: white;
  }
  
  .action-btn.danger {
    background: var(--brewos-danger);
    color: white;
  }
  
  .action-btn:disabled {
    opacity: 0.5;
    cursor: not-allowed;
  }
  
  /* Stats Footer */
  .stats-footer {
    display: flex;
    justify-content: space-around;
    padding-top: 16px;
    border-top: 1px solid var(--divider-color, #e5e7eb);
    margin-top: 16px;
  }
  
  .stat-item {
    text-align: center;
  }
  
  .stat-item .value {
    font-size: 18px;
    font-weight: 600;
    color: var(--brewos-text);
  }
  
  .stat-item .label {
    font-size: 11px;
    color: var(--brewos-text-secondary);
  }
  
  /* Unavailable state */
  .unavailable {
    text-align: center;
    padding: 32px;
    color: var(--brewos-text-secondary);
  }
  
  .unavailable ha-icon {
    --mdc-icon-size: 48px;
    margin-bottom: 12px;
    opacity: 0.5;
  }
`;

class BrewOSCard extends HTMLElement {
  constructor() {
    super();
    this.attachShadow({ mode: 'open' });
  }

  static get properties() {
    return {
      hass: {},
      config: {},
    };
  }

  setConfig(config) {
    if (!config.entity && !config.device_id) {
      throw new Error('Please define an entity or device_id');
    }
    
    this.config = {
      name: 'Espresso Machine',
      show_name: true,
      show_stats: true,
      show_actions: true,
      compact: false,
      ...config
    };
  }

  set hass(hass) {
    this._hass = hass;
    this.render();
  }

  getEntityValue(entityId, attribute = null) {
    if (!this._hass || !entityId) return null;
    const entity = this._hass.states[entityId];
    if (!entity) return null;
    if (attribute) {
      return entity.attributes[attribute];
    }
    return entity.state;
  }

  getEntity(suffix) {
    const prefix = this.config.entity_prefix || 'brewos';
    return `sensor.${prefix}_${suffix}`;
  }

  getBinaryEntity(suffix) {
    const prefix = this.config.entity_prefix || 'brewos';
    return `binary_sensor.${prefix}_${suffix}`;
  }

  getSwitchEntity(suffix) {
    const prefix = this.config.entity_prefix || 'brewos';
    return `switch.${prefix}_${suffix}`;
  }

  getNumberEntity(suffix) {
    const prefix = this.config.entity_prefix || 'brewos';
    return `number.${prefix}_${suffix}`;
  }

  getSelectEntity(suffix) {
    const prefix = this.config.entity_prefix || 'brewos';
    return `select.${prefix}_${suffix}`;
  }

  getButtonEntity(suffix) {
    const prefix = this.config.entity_prefix || 'brewos';
    return `button.${prefix}_${suffix}`;
  }

  formatTemp(value) {
    if (value === null || value === undefined || value === 'unavailable' || value === 'unknown') {
      return '--';
    }
    return parseFloat(value).toFixed(1);
  }

  formatTime(seconds) {
    if (!seconds || seconds === 'unavailable') return '00:00';
    const s = parseFloat(seconds);
    const mins = Math.floor(s / 60);
    const secs = Math.floor(s % 60);
    return `${mins.toString().padStart(2, '0')}:${secs.toString().padStart(2, '0')}`;
  }

  getStatusClass() {
    const brewing = this.getEntityValue(this.getBinaryEntity('is_brewing'));
    const heating = this.getEntityValue(this.getBinaryEntity('is_heating'));
    const ready = this.getEntityValue(this.getBinaryEntity('ready'));
    const fault = this.getEntityValue(this.getBinaryEntity('alarm_active'));
    
    if (fault === 'on') return 'fault';
    if (brewing === 'on') return 'brewing';
    if (ready === 'on') return 'ready';
    if (heating === 'on') return 'heating';
    return 'standby';
  }

  getStatusText() {
    const statusClass = this.getStatusClass();
    const texts = {
      fault: 'Fault',
      brewing: 'Brewing',
      ready: 'Ready',
      heating: 'Heating',
      standby: 'Standby'
    };
    return texts[statusClass] || 'Unknown';
  }

  getHeatingStrategy() {
    const strategyEntity = this.getSelectEntity('heating_strategy');
    const value = this.getEntityValue(strategyEntity);
    const strategies = {
      'brew_only': 'Brew Only',
      'sequential': 'Sequential',
      'parallel': 'Parallel',
      'smart_stagger': 'Smart Stagger'
    };
    return strategies[value] || value || 'Sequential';
  }

  getSelectEntity(suffix) {
    const prefix = this.config.entity_prefix || 'brewos';
    return `select.${prefix}_${suffix}`;
  }

  isPowered() {
    const switchState = this.getEntityValue(this.getSwitchEntity('power_switch'));
    return switchState === 'on';
  }

  isBrewing() {
    return this.getEntityValue(this.getBinaryEntity('is_brewing')) === 'on';
  }

  handlePowerToggle() {
    const switchEntity = this.getSwitchEntity('power_switch');
    const service = this.isPowered() ? 'turn_off' : 'turn_on';
    this._hass.callService('switch', service, { entity_id: switchEntity });
  }

  handleStartBrew() {
    const buttonEntity = this.getButtonEntity('start_brew');
    this._hass.callService('button', 'press', { entity_id: buttonEntity });
  }

  handleStopBrew() {
    const buttonEntity = this.getButtonEntity('stop_brew');
    this._hass.callService('button', 'press', { entity_id: buttonEntity });
  }

  handleTare() {
    const buttonEntity = this.getButtonEntity('tare_scale');
    this._hass.callService('button', 'press', { entity_id: buttonEntity });
  }

  render() {
    if (!this._hass) return;

    const brewTemp = this.getEntityValue(this.getEntity('brew_temp'));
    const brewSetpoint = this.getEntityValue(this.getEntity('brew_setpoint'));
    const steamTemp = this.getEntityValue(this.getEntity('steam_temp'));
    const steamSetpoint = this.getEntityValue(this.getEntity('steam_setpoint'));
    const pressure = this.getEntityValue(this.getEntity('pressure'));
    const shotDuration = this.getEntityValue(this.getEntity('shot_duration'));
    const shotWeight = this.getEntityValue(this.getEntity('shot_weight'));
    const flowRate = this.getEntityValue(this.getEntity('flow_rate'));
    const shotsToday = this.getEntityValue(this.getEntity('shots_today'));
    const totalShots = this.getEntityValue(this.getEntity('total_shots'));
    const scaleConnected = this.getEntityValue(this.getBinaryEntity('scale_connected'));
    
    const statusClass = this.getStatusClass();
    const statusText = this.getStatusText();
    const isPowered = this.isPowered();
    const isBrewing = this.isBrewing();
    
    // Calculate temp progress
    const brewProgress = brewSetpoint > 0 ? Math.min(100, (parseFloat(brewTemp) / parseFloat(brewSetpoint)) * 100) : 0;
    const steamProgress = steamSetpoint > 0 ? Math.min(100, (parseFloat(steamTemp) / parseFloat(steamSetpoint)) * 100) : 0;

    // Check if entity is available
    const isAvailable = brewTemp !== null && brewTemp !== 'unavailable';

    this.shadowRoot.innerHTML = `
      <style>${styles}</style>
      <ha-card>
        <div class="brewos-card">
          ${!isAvailable ? `
            <div class="unavailable">
              <ha-icon icon="mdi:coffee-maker-outline"></ha-icon>
              <div>Machine Unavailable</div>
            </div>
          ` : `
            <!-- Header -->
            <div class="header">
              <div class="header-left">
                <div class="machine-icon">☕</div>
                <div class="machine-info">
                  ${this.config.show_name ? `<h2>${this.config.name}</h2>` : ''}
                  <div class="status">
                    <span class="status-indicator ${statusClass}"></span>
                    <span class="status-text">${statusText}${isPowered ? ` · ${this.getHeatingStrategy()}` : ''}</span>
                  </div>
                </div>
              </div>
              <button class="power-toggle ${isPowered ? 'on' : 'off'}" @click="${() => this.handlePowerToggle()}">
                <ha-icon icon="mdi:power"></ha-icon>
                ${isPowered ? 'On' : 'Off'}
              </button>
            </div>
            
            <!-- Shot Timer (when brewing) -->
            ${isBrewing ? `
              <div class="shot-timer">
                <div class="timer">${this.formatTime(shotDuration)}</div>
                <div class="shot-stats">
                  <div class="shot-stat">
                    <div class="value">${this.formatTemp(shotWeight)}g</div>
                    <div class="label">Weight</div>
                  </div>
                  <div class="shot-stat">
                    <div class="value">${this.formatTemp(flowRate)}</div>
                    <div class="label">Flow (g/s)</div>
                  </div>
                  <div class="shot-stat">
                    <div class="value">${this.formatTemp(pressure)}</div>
                    <div class="label">Bar</div>
                  </div>
                </div>
              </div>
            ` : ''}
            
            <!-- Temperatures -->
            <div class="temps-section">
              <div class="temp-card brew">
                <div class="temp-label">Brew Boiler</div>
                <div class="temp-value">${this.formatTemp(brewTemp)}<span class="unit">°C</span></div>
                <div class="temp-setpoint">Target: ${this.formatTemp(brewSetpoint)}°C</div>
                <div class="temp-progress" style="width: ${brewProgress}%"></div>
              </div>
              <div class="temp-card steam">
                <div class="temp-label">Steam Boiler</div>
                <div class="temp-value">${this.formatTemp(steamTemp)}<span class="unit">°C</span></div>
                <div class="temp-setpoint">Target: ${this.formatTemp(steamSetpoint)}°C</div>
                <div class="temp-progress" style="width: ${steamProgress}%"></div>
              </div>
            </div>
            
            <!-- Metrics -->
            ${!isBrewing ? `
              <div class="metrics-row">
                <div class="metric-card">
                  <div class="metric-icon">⚡</div>
                  <div class="metric-value">${this.formatTemp(pressure)}</div>
                  <div class="metric-label">Bar</div>
                </div>
                <div class="metric-card">
                  <div class="metric-icon">⚖️</div>
                  <div class="metric-value">${scaleConnected === 'on' ? this.formatTemp(shotWeight) : '--'}</div>
                  <div class="metric-label">Weight (g)</div>
                </div>
                <div class="metric-card">
                  <div class="metric-icon">💧</div>
                  <div class="metric-value">${scaleConnected === 'on' ? this.formatTemp(flowRate) : '--'}</div>
                  <div class="metric-label">Flow (g/s)</div>
                </div>
              </div>
            ` : ''}
            
            <!-- Actions -->
            ${this.config.show_actions && isPowered ? `
              <div class="actions">
                ${!isBrewing ? `
                  <button class="action-btn primary" @click="${() => this.handleStartBrew()}">
                    <ha-icon icon="mdi:coffee"></ha-icon>
                    Start Brew
                  </button>
                ` : `
                  <button class="action-btn danger" @click="${() => this.handleStopBrew()}">
                    <ha-icon icon="mdi:stop"></ha-icon>
                    Stop Brew
                  </button>
                `}
                <button class="action-btn" @click="${() => this.handleTare()}" ${scaleConnected !== 'on' ? 'disabled' : ''}>
                  <ha-icon icon="mdi:scale-balance"></ha-icon>
                  Tare Scale
                </button>
              </div>
            ` : ''}
            
            <!-- Stats Footer -->
            ${this.config.show_stats ? `
              <div class="stats-footer">
                <div class="stat-item">
                  <div class="value">${shotsToday || '0'}</div>
                  <div class="label">Today</div>
                </div>
                <div class="stat-item">
                  <div class="value">${totalShots || '0'}</div>
                  <div class="label">Total Shots</div>
                </div>
                <div class="stat-item">
                  <div class="value">${scaleConnected === 'on' ? '✓' : '✗'}</div>
                  <div class="label">Scale</div>
                </div>
              </div>
            ` : ''}
          `}
        </div>
      </ha-card>
    `;
    
    // Bind event handlers after rendering
    this.shadowRoot.querySelectorAll('.power-toggle').forEach(el => {
      el.addEventListener('click', () => this.handlePowerToggle());
    });
    this.shadowRoot.querySelectorAll('.action-btn.primary').forEach(el => {
      el.addEventListener('click', () => this.handleStartBrew());
    });
    this.shadowRoot.querySelectorAll('.action-btn.danger').forEach(el => {
      el.addEventListener('click', () => this.handleStopBrew());
    });
    this.shadowRoot.querySelectorAll('.action-btn:not(.primary):not(.danger)').forEach(el => {
      if (!el.disabled) {
        el.addEventListener('click', () => this.handleTare());
      }
    });
  }

  getCardSize() {
    return this.config.compact ? 3 : 5;
  }

  static getConfigElement() {
    return document.createElement('brewos-card-editor');
  }

  static getStubConfig() {
    return {
      entity_prefix: 'brewos',
      name: 'Espresso Machine',
      show_name: true,
      show_stats: true,
      show_actions: true
    };
  }
}

// Card Editor
class BrewOSCardEditor extends HTMLElement {
  setConfig(config) {
    this._config = config;
    this.render();
  }

  render() {
    if (!this._config) return;

    this.innerHTML = `
      <div style="padding: 16px;">
        <ha-textfield
          label="Entity Prefix"
          .value="${this._config.entity_prefix || 'brewos'}"
          @change="${(e) => this._valueChanged('entity_prefix', e.target.value)}"
        ></ha-textfield>
        
        <ha-textfield
          label="Name"
          .value="${this._config.name || 'Espresso Machine'}"
          @change="${(e) => this._valueChanged('name', e.target.value)}"
          style="margin-top: 8px;"
        ></ha-textfield>
        
        <ha-formfield label="Show Name" style="margin-top: 16px;">
          <ha-switch
            .checked="${this._config.show_name !== false}"
            @change="${(e) => this._valueChanged('show_name', e.target.checked)}"
          ></ha-switch>
        </ha-formfield>
        
        <ha-formfield label="Show Stats" style="margin-top: 8px;">
          <ha-switch
            .checked="${this._config.show_stats !== false}"
            @change="${(e) => this._valueChanged('show_stats', e.target.checked)}"
          ></ha-switch>
        </ha-formfield>
        
        <ha-formfield label="Show Actions" style="margin-top: 8px;">
          <ha-switch
            .checked="${this._config.show_actions !== false}"
            @change="${(e) => this._valueChanged('show_actions', e.target.checked)}"
          ></ha-switch>
        </ha-formfield>
      </div>
    `;
  }

  _valueChanged(key, value) {
    if (!this._config) return;
    const newConfig = { ...this._config, [key]: value };
    this._config = newConfig;
    this.dispatchEvent(new CustomEvent('config-changed', { detail: { config: newConfig } }));
  }
}

customElements.define('brewos-card', BrewOSCard);
customElements.define('brewos-card-editor', BrewOSCardEditor);

window.customCards = window.customCards || [];
window.customCards.push({
  type: 'brewos-card',
  name: 'BrewOS Card',
  description: 'A beautiful card for your BrewOS espresso machine',
  preview: true,
  documentationURL: 'https://github.com/your-repo/brewos'
});

console.info(`%c BREWOS-CARD %c ${CARD_VERSION} `, 
  'color: white; background: #d97706; font-weight: bold; padding: 2px 6px; border-radius: 4px 0 0 4px;',
  'color: #d97706; background: #fef3c7; font-weight: bold; padding: 2px 6px; border-radius: 0 4px 4px 0;'
);

