<script setup>
import { connectionState, connectUSB, connectBLE, disconnectDevice, transportType } from '../lib/store.js'
import { isWebSerialAvailable } from '../lib/serial.js'
import { isWebBluetoothAvailable } from '../lib/ble.js'

const webSerialOk = isWebSerialAvailable()
const webBluetoothOk = isWebBluetoothAvailable()
</script>

<template>
  <div class="connect-section">
    <template v-if="!webSerialOk && !webBluetoothOk">
      <div class="warning">
        Web Serial and Web Bluetooth are not available. Use Chrome or Edge on desktop.
      </div>
    </template>
    <template v-else-if="!connectionState.connected">
      <div class="connect-buttons">
        <button
          v-if="webSerialOk"
          class="btn btn-connect btn-usb"
          :disabled="connectionState.connecting"
          @click="connectUSB"
        >
          {{ connectionState.connecting ? 'Connecting...' : 'Connect USB' }}
        </button>
        <button
          v-if="webBluetoothOk"
          class="btn btn-connect btn-ble"
          :disabled="connectionState.connecting"
          @click="connectBLE"
        >
          {{ connectionState.connecting ? 'Connecting...' : 'Connect Bluetooth' }}
        </button>
      </div>
      <p v-if="webBluetoothOk" class="bt-hint">
        Don't see the device in Chrome's Bluetooth device list? Go to your OS Bluetooth settings and remove (forget) the device first, and then connect here to pair with the device with Chrome. Once it's paired with Chrome, future Dashboard visits just need a disconnect (not forget) on the OS side come back here and Chrome will recongize the device again (it should say "- paired" on Chrome's device list).
      </p>
      <div v-if="connectionState.error" class="error">
        {{ connectionState.error }}
      </div>
    </template>
    <template v-else>
      <button
        class="btn btn-disconnect"
        @click="disconnectDevice"
      >
        Disconnect ({{ connectionState.deviceName }})
        <span class="transport-badge" :class="transportType">
          {{ transportType === 'ble' ? 'BT' : 'USB' }}
        </span>
      </button>
    </template>
  </div>
</template>

<style scoped>
.connect-section {
  text-align: center;
  padding: 1rem 0;
}
.connect-buttons {
  display: flex;
  justify-content: center;
  gap: 0.75rem;
}
.btn-ble {
  background: #7c4dff;
  border-color: #7c4dff;
}
.btn-ble:hover:not(:disabled) {
  background: #651fff;
  border-color: #651fff;
}
.transport-badge {
  display: inline-block;
  font-size: 0.7rem;
  font-weight: 700;
  padding: 0.1rem 0.4rem;
  border-radius: 4px;
  margin-left: 0.5rem;
  vertical-align: middle;
  text-transform: uppercase;
  letter-spacing: 0.03em;
}
.transport-badge.usb {
  background: rgba(79, 195, 247, 0.15);
  color: var(--accent);
}
.transport-badge.ble {
  background: rgba(124, 77, 255, 0.15);
  color: #b388ff;
}
.warning {
  background: #fff3cd;
  color: #856404;
  padding: 0.75rem 1rem;
  border-radius: 6px;
  font-size: 0.9rem;
}
.bt-hint {
  margin-top: 0.75rem;
  font-size: 0.8rem;
  color: var(--text-dim);
  line-height: 1.5;
}
.error {
  color: #dc3545;
  margin-top: 0.5rem;
  font-size: 0.85rem;
}
</style>
