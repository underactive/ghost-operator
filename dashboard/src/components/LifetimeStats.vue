<script setup>
import { status } from '../lib/store.js'

const PIXELS_PER_FOOT = 1152
const PIXELS_PER_METER = 3780
const PIXELS_PER_TENTH_MILE = 608256
const PIXELS_PER_TENTH_KM = 377953

function formatCount(n) {
  if (n < 10000) return n.toLocaleString()
  if (n < 1000000) return Math.floor(n / 1000) + 'k'
  return (n / 1000000).toFixed(1) + 'M'
}

function formatDistance(px) {
  const feet = Math.floor(px / PIXELS_PER_FOOT)
  if (feet < 5280) {
    const meters = Math.floor(px / PIXELS_PER_METER)
    return `${feet.toLocaleString()} ft / ${meters.toLocaleString()} m`
  }
  const tMi = Math.floor(px / PIXELS_PER_TENTH_MILE)
  const tKm = Math.floor(px / PIXELS_PER_TENTH_KM)
  const mi = tMi < 100 ? (tMi / 10).toFixed(1) : Math.floor(tMi / 10)
  const km = tKm < 100 ? (tKm / 10).toFixed(1) : Math.floor(tKm / 10)
  return `${mi} mi / ${km} km`
}
</script>

<template>
  <section class="card">
    <h2>Lifetime Stats</h2>
    <div class="stats-grid">
      <div class="stat">
        <span class="stat-value">{{ formatCount(status.totalKeys) }}</span>
        <span class="stat-label">Keys Sent</span>
      </div>
      <div class="stat">
        <span class="stat-value">{{ formatDistance(status.totalMousePx) }}</span>
        <span class="stat-label">Mouse Distance</span>
      </div>
      <div class="stat">
        <span class="stat-value">{{ formatCount(status.totalClicks) }}</span>
        <span class="stat-label">Mouse Clicks</span>
      </div>
    </div>
  </section>
</template>

<style scoped>
.stats-grid {
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  gap: 1rem;
}

.stat {
  display: flex;
  flex-direction: column;
  align-items: center;
  padding: 0.75rem 0.5rem;
  background: var(--surface-2);
  border-radius: 8px;
}

.stat-value {
  font-size: 1.25rem;
  font-weight: 700;
  font-family: 'SF Mono', 'Fira Code', monospace;
  color: var(--accent);
}

.stat-label {
  font-size: 0.75rem;
  color: var(--text-dim);
  margin-top: 0.25rem;
  text-transform: uppercase;
  letter-spacing: 0.05em;
}
</style>
