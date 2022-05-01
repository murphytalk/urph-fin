<style lang="postcss" global>
  @tailwind base;
  @tailwind components;
  @tailwind utilities;
</style>

<script lang="ts">
	import Overview from './components/Overview.svelte';
  import Funds from './components/Funds.svelte';
	import ModalDlg from './share/ModalDlg.svelte';
  import { Router, Route, link} from "svelte-navigator";
  import Fa from 'svelte-fa'
  import { faGaugeHigh, faSackDollar, faChartLine, faBitcoinSign} from '@fortawesome/free-solid-svg-icons'  
  let aboutWndIsVisible = false;
</script>

<div class="flex flex-col h-full">
<div>
  <nav class="bg-white dark:bg-gray-800 shadow">
    <div class="max-w-7xl px-2">
      <div class="flex items-center justify-between h-12">
        <div class="flex items-center">
          <a class="flex-shrink-0" href="#" on:click="{() => aboutWndIsVisible = true }">
            <img class="h-8 w-8" src="images/crab.png" alt="Logo" />
          </a>
          <div class="hidden md:block">
            <div class="ml-2 flex items-baseline space-x-2">
              <a
                class="text-gray-800  dark:text-white  hover:text-gray-800 dark:hover:text-white px-3 py-2 rounded-md text-sm font-medium"
                href="/" use:link
              >
               <div class="flex justify-between items-center">
                  <div class="text-xl px-2"><Fa icon={faGaugeHigh} /></div>
                  <div>Dashboard</div>
                </div>
              </a>
              <a
                class="text-gray-300  hover:text-gray-800 dark:hover:text-white px-3 py-2 rounded-md text-sm font-medium"
                href="funds" use:link
              >
               <div class="flex justify-between items-center">
                  <div class="text-xl px-2"><Fa icon={faSackDollar} /></div>
                  <div>Funds</div>
                </div>
              </a>
              <a
                class="text-gray-300  hover:text-gray-800 dark:hover:text-white px-3 py-2 rounded-md text-sm font-medium"
                href="/" use:link
              >
               <div class="flex justify-between items-center">
                  <div class="text-xl px-2"><Fa icon={faChartLine} /></div>
                  <div>Stocks</div>
                </div>
              </a>
              <a
                class="text-gray-300  hover:text-gray-800 dark:hover:text-white px-3 py-2 rounded-md text-sm font-medium"
                href="/" use:link
              >
               <div class="flex justify-between items-center">
                  <div class="text-xl px-2"><Fa icon={faBitcoinSign} /></div>
                  <div>Crypto</div>
               </div>
              </a>
            </div>
          </div>
        </div>
      </div>
    </div>
  </nav>
</div>

<Router>
    <Route path="/">
      <Overview/>
  </Route>
    <Route path="funds">
      <Funds/>
  </Route>
</Router>
</div>

<ModalDlg bind:shown={aboutWndIsVisible}>
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
