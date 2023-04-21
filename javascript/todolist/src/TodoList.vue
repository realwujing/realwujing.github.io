<template>
  <div>
    <div>
    <input class="item" v-model="inputValue"/>
    <button @click="handleSubmit">提交</button>
  </div>
  <ul>
    <todo-item 
    v-for="(item, index) of list"
    :content="item"
    :key="index"
    :index="index"
    @delete="handleDelete"
    >
    </todo-item>
  </ul>
  </div>
</template>

<script>
import TodoItem from './components/TodoItem'
export default {
  components: {
    'todo-item': TodoItem
  },
  data () {
    return {
      inputValue: '',
      list: []
    }
  },
  methods: {
    handleSubmit () {
      this.list.push(this.inputValue)
      this.inputValue = ''
    },
    handleDelete (index) {
      this.list.splice(index, 1)
    }
  }
}
</script>

<style>

</style>
