<script setup>
import { computed } from 'vue'
import { batteryHistory, status } from '../lib/store.js'

const PAD = { top: 20, right: 50, bottom: 24, left: 40 }
const W = 640
const H = 180
const PLOT_W = W - PAD.left - PAD.right
const PLOT_H = H - PAD.top - PAD.bottom

// Voltage axis range
const MV_MIN = 3200
const MV_MAX = 4200

function pctY(pct) {
  return PAD.top + PLOT_H * (1 - pct / 100)
}

function mvY(mv) {
  return PAD.top + PLOT_H * (1 - (mv - MV_MIN) / (MV_MAX - MV_MIN))
}

const hasData = computed(() => batteryHistory.length >= 2)

const timeRange = computed(() => {
  if (!hasData.value) return { min: 0, max: 1 }
  return {
    min: batteryHistory[0].t,
    max: batteryHistory[batteryHistory.length - 1].t,
  }
})

function timeX(t) {
  const { min, max } = timeRange.value
  const span = max - min || 1
  return PAD.left + PLOT_W * ((t - min) / span)
}

const pctPath = computed(() => {
  if (!hasData.value) return ''
  return batteryHistory.map((e, i) => {
    const x = timeX(e.t).toFixed(1)
    const y = pctY(e.pct).toFixed(1)
    return `${i === 0 ? 'M' : 'L'}${x},${y}`
  }).join(' ')
})

const mvPath = computed(() => {
  if (!hasData.value) return ''
  return batteryHistory.map((e, i) => {
    const x = timeX(e.t).toFixed(1)
    const y = mvY(e.mv).toFixed(1)
    return `${i === 0 ? 'M' : 'L'}${x},${y}`
  }).join(' ')
})

const gridLines = computed(() => [0, 25, 50, 75, 100].map(pct => ({
  y: pctY(pct),
  label: `${pct}%`,
  mvLabel: ((MV_MIN + (MV_MAX - MV_MIN) * pct / 100) / 1000).toFixed(2) + 'V',
})))

const timeLabels = computed(() => {
  if (!hasData.value) return []
  const { min, max } = timeRange.value
  const span = max - min
  if (span < 10000) return [] // less than 10s, skip labels
  const labels = []
  // Show 4 evenly-spaced time labels
  for (let i = 0; i <= 3; i++) {
    const t = min + (span * i / 3)
    const ago = Math.round((max - t) / 1000)
    let text
    if (ago <= 0) text = 'now'
    else if (ago < 60) text = `${ago}s ago`
    else text = `${Math.round(ago / 60)}m ago`
    labels.push({ x: timeX(t), text })
  }
  return labels
})

const currentPct = computed(() => {
  if (!hasData.value) return ''
  return batteryHistory[batteryHistory.length - 1].pct + '%'
})

const currentMv = computed(() => {
  if (!hasData.value) return ''
  return (batteryHistory[batteryHistory.length - 1].mv / 1000).toFixed(2) + 'V'
})
</script>

<template>
  <div class="card battery-chart">
    <h2>Battery History</h2>
    <div v-if="!hasData" class="no-data">
      <span>No data yet — chart populates as status polls arrive</span>
    </div>
    <svg v-else :viewBox="`0 0 ${W} ${H}`" preserveAspectRatio="xMidYMid meet" class="chart-svg">
      <!-- Grid lines -->
      <line
        v-for="g in gridLines" :key="'g' + g.label"
        :x1="PAD.left" :x2="W - PAD.right"
        :y1="g.y" :y2="g.y"
        class="grid-line"
      />
      <!-- Left Y-axis labels (%) -->
      <text
        v-for="g in gridLines" :key="'lbl' + g.label"
        :x="PAD.left - 4" :y="g.y + 3"
        class="axis-label" text-anchor="end"
      >{{ g.label }}</text>
      <!-- Right Y-axis labels (V) -->
      <text
        v-for="g in gridLines" :key="'mv' + g.mvLabel"
        :x="W - PAD.right + 4" :y="g.y + 3"
        class="axis-label axis-mv" text-anchor="start"
      >{{ g.mvLabel }}</text>
      <!-- X-axis time labels -->
      <text
        v-for="tl in timeLabels" :key="tl.text"
        :x="tl.x" :y="H - 4"
        class="axis-label" text-anchor="middle"
      >{{ tl.text }}</text>
      <!-- Percentage line -->
      <path :d="pctPath" class="line-pct" />
      <!-- Voltage line -->
      <path :d="mvPath" class="line-mv" />
      <!-- Current value indicators -->
      <text :x="W - PAD.right + 4" :y="PAD.top - 6" class="cur-label cur-pct" text-anchor="start">{{ currentPct }}</text>
      <text :x="W - PAD.right + 4" :y="PAD.top - 6 + 12" class="cur-label cur-mv" text-anchor="start">{{ currentMv }}</text>
    </svg>
    <div class="chart-legend">
      <span class="legend-pct">&#9644; Battery %</span>
      <span class="legend-mv">&#9644; Voltage</span>
    </div>
  </div>
</template>

<style scoped>
.battery-chart {
  padding-bottom: 0.5rem;
}
.no-data {
  text-align: center;
  padding: 1.5rem 0;
  color: var(--text-dim);
  font-size: 0.85rem;
}
.chart-svg {
  width: 100%;
  height: auto;
  display: block;
}
.grid-line {
  stroke: var(--border);
  stroke-width: 0.5;
  stroke-dasharray: 4 4;
}
.axis-label {
  fill: var(--text-dim);
  font-size: 9px;
  font-family: 'SF Mono', 'Fira Code', monospace;
}
.axis-mv {
  fill: #666;
}
.line-pct {
  fill: none;
  stroke: var(--accent);
  stroke-width: 1.5;
  stroke-linejoin: round;
}
.line-mv {
  fill: none;
  stroke: #666;
  stroke-width: 1;
  stroke-linejoin: round;
  stroke-dasharray: 3 2;
}
.cur-label {
  font-size: 10px;
  font-family: 'SF Mono', 'Fira Code', monospace;
  font-weight: 600;
}
.cur-pct { fill: var(--accent); }
.cur-mv { fill: #888; }
.chart-legend {
  display: flex;
  justify-content: center;
  gap: 1.5rem;
  font-size: 0.75rem;
  color: var(--text-dim);
  padding-top: 0.25rem;
}
.legend-pct { color: var(--accent); }
.legend-mv { color: #888; }
</style>
