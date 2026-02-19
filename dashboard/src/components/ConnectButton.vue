<script setup>
import { connectionState, connectDevice, disconnectDevice } from '../lib/store.js'
import { isWebSerialAvailable } from '../lib/serial.js'

const webSerialAvailable = isWebSerialAvailable()
</script>

<template>
  <div class="connect-section">
    <template v-if="!webSerialAvailable">
      <div class="warning">
        Web Serial is not available. Use Chrome or Edge on desktop.
      </div>
    </template>
    <template v-else>
      <button
        v-if="!connectionState.connected"
        class="btn btn-connect"
        :disabled="connectionState.connecting"
        @click="connectDevice"
      >
        {{ connectionState.connecting ? 'Connecting...' : 'Connect USB' }}
      </button>
      <button
        v-else
        class="btn btn-disconnect"
        @click="disconnectDevice"
      >
        Disconnect ({{ connectionState.deviceName }})
      </button>
      <div v-if="connectionState.error" class="error">
        {{ connectionState.error }}
      </div>
    </template>
  </div>
</template>

<style scoped>
.connect-section {
  text-align: center;
  padding: 1rem 0;
}
.warning {
  background: #fff3cd;
  color: #856404;
  padding: 0.75rem 1rem;
  border-radius: 6px;
  font-size: 0.9rem;
}
.error {
  color: #dc3545;
  margin-top: 0.5rem;
  font-size: 0.85rem;
}
</style>
