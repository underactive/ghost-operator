<script setup>
import { settings, setSetting } from '../lib/store.js'
import { KEY_SOUND_NAMES } from '../lib/protocol.js'
</script>

<template>
  <section class="card">
    <h2>Sound</h2>

    <div class="field">
      <label>Key Sound</label>
      <select :value="settings.sound" @change="setSetting('sound', Number($event.target.value))">
        <option :value="0">Off</option>
        <option :value="1">On</option>
      </select>
      <p class="help-text">Play a keyboard switch sound on each keystroke through the piezo buzzer.</p>
    </div>

    <div class="field" v-if="settings.sound === 1">
      <label>Sound Type</label>
      <select
        :value="settings.soundType"
        @change="setSetting('soundType', Number($event.target.value))"
      >
        <option v-for="(name, idx) in KEY_SOUND_NAMES" :key="idx" :value="idx">
          {{ name }}
        </option>
      </select>
    </div>
  </section>
</template>
