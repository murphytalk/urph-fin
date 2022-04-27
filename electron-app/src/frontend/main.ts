import "svelte";
import App from "./App.svelte";
import 'tw-elements';

const app = new App({
  target: document.body,
  props: {
    name: "world",
  },
});

export default app;
