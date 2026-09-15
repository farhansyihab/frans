<template>
  <button @click="handleClick">{{ label }}</button>
</template>

<script setup lang="ts">
import { ref } from 'vue'

const label = ref('Click me')

function handleClick() {
  console.log('clicked', label.value)
}
</script>

<style scoped>
button { color: blue; }
</style>
