<template>
  <button @click="handleClick">{{ label }}</button>
</template>

<script>
function handleClick() {
  console.log('clicked')
}

export default {
  name: 'LegacyButton',
  methods: {
    handleClick,
  }
}
</script>
