<template>
  <div>{{ msg }}</div>
</template>

<script lang="tsx">
import { ref } from 'vue'

const msg = ref('hello')

function renderTemplate(text: string) {
  return <span>{text}</span>
}

export default {
  name: 'TsxVue',
  setup() {
    return { msg, renderTemplate }
  }
}
</script>
