<style lang="postcss" global>
  @tailwind base;
  @tailwind components;
  @tailwind utilities;

  main {
    text-align: center;
    padding: 1em;
    max-width: 240px;
    margin: 0 auto;
  }
  @media (min-width: 640px) {
    main {
      max-width: none;
    }
  }
</style>

<script lang="ts">
  //import Icon from "svelte-awesome";
  //import { infoCircle } from "svelte-awesome/icons";
</script>

<div>
  <nav class="bg-white dark:bg-gray-800  shadow ">
    <div class="max-w-7xl mx-auto px-2">
      <div class="flex items-center justify-between h-12">
        <div class=" flex items-center">
          <a class="flex-shrink-0" href="/">
            <img class="h-8 w-8" src="images/crab.png" alt="Logo" />
          </a>
          <div class="hidden md:block">
            <div class="ml-10 flex items-baseline space-x-4">
              <a
                class="text-gray-800  dark:text-white  hover:text-gray-800 dark:hover:text-white px-3 py-2 rounded-md text-sm font-medium"
                href="/#"
              >
                Dashboard
              </a>
              <a
                class="text-gray-300  hover:text-gray-800 dark:hover:text-white px-3 py-2 rounded-md text-sm font-medium"
                href="/#"
              >
                Funds
              </a>
              <a
                class="text-gray-300  hover:text-gray-800 dark:hover:text-white px-3 py-2 rounded-md text-sm font-medium"
                href="/#"
              >
                Stocks
              </a>
              <a
                class="text-gray-300  hover:text-gray-800 dark:hover:text-white px-3 py-2 rounded-md text-sm font-medium"
                href="/#"
              >
                Crypto
              </a>
            </div>
          </div>
        </div>
      </div>
    </div>
  </nav>
</div>
