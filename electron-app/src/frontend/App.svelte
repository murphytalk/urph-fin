<style lang="postcss" global>
  @tailwind base;
  @tailwind components;
  @tailwind utilities;

  #modal-overlay {
  width: 100%;
  height: 100%;
  position: fixed;
  top: 0;
  left: 0;
  z-index: 0;
  background-color: black;
  opacity: 0.5;  
}  
</style>

<script lang="ts">
	import ModalDlg from './share/ModalDlg.svelte';
  //import Icon from "svelte-awesome";
  //import { infoCircle } from "svelte-awesome/icons";
  let aboutVisible = false;

  function showAbout(): void{
    aboutVisible = true;
  }
</script>

<div>
  <nav class="bg-white dark:bg-gray-800 shadow">
    <div class="max-w-7xl px-2">
      <div class="flex items-center justify-between h-12">
        <div class="flex items-center">
          <a class="flex-shrink-0" href="#" on:click="{showAbout}">
            <img class="h-8 w-8" src="images/crab.png" alt="Logo" />
          </a>
          <div class="hidden md:block">
            <div class="ml-2 flex items-baseline space-x-2">
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

<ModalDlg bind:shown={aboutVisible}>
		<div
			class="mx-auto flex items-center justify-center h-12 w-12 rounded-full bg-blue-100"
		>
			<svg
				class="h-6 w-6 text-blue-600"
				fill="none"
				stroke="currentColor"
				viewBox="0 0 24 24"
				xmlns="http://www.w3.org/2000/svg"
			>
				<path
					stroke-linecap="round"
					stroke-linejoin="round"
					stroke-width="2"
					d="M5 13l4 4L19 7"
				></path>
			</svg>
		</div>
		<h3 class="text-lg leading-6 font-medium text-gray-900">Successful!</h3>
		<div class="mt-2 px-7 py-3">
			<p class="text-sm text-gray-500">
				Account has been successfully registered!
			</p>
		</div>
</ModalDlg>
