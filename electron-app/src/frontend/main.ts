import "svelte";
import App from "./App.svelte";
let ap: string = 'dd';
console.log(ap);
const app = new App({
  target: document.body,
  props: {
  },
});

export default app;
