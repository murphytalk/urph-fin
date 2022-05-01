<style lang="postcss" global>
  @tailwind base;
  @tailwind components;
  @tailwind utilities;

  .inactive-nav{
    @apply text-gray-300  hover:text-blue-800 dark:hover:text-white px-3 py-2 rounded-md text-sm font-medium;
  }
  .active-nav{
    @apply text-green-600  hover:text-blue-500 dark:hover:text-white px-3 py-2 rounded-md text-sm font-medium;
  }
</style>

<script lang="ts">
	import Overview from './components/Overview.svelte';
  import Funds from './components/Funds.svelte';
  import Crypto from './components/Crypto.svelte';
  import Stocks from './components/Stocks.svelte';
	import ModalDlg from './share/ModalDlg.svelte';
  import { Router, Route, link } from "svelte-navigator";
  import Fa from 'svelte-fa'
  import { faGaugeHigh, faSackDollar, faChartLine, faBitcoinSign, faCircleInfo} from '@fortawesome/free-solid-svg-icons'  

  let aboutWndIsVisible = false;
  let activeComponent: Overview|Funds|Stocks|Crypto;

  $: selected =  (activeComponent && activeComponent.constructor) ? activeComponent.constructor.name : 'Overview';
</script>

<div class="flex flex-col h-full">
  <nav class="bg-white dark:bg-gray-800 mb-1 shadow flex justify-between">
      <div class="flex flex-1 justify-between h-12 items-center">
          <div class="flex items-center">
            <div class="ml-1 flex items-baseline space-x-2">
              <a
                class="{ selected === 'Overview' ? 'active-nav' : 'inactive-nav' }"
                href="/" use:link
              >
               <div class="flex justify-between items-center">
                  <div class="text-xl px-2"><Fa icon={faGaugeHigh} /></div>
                  <div>Dashboard</div>
                </div>
              </a>
              <a
                class="{ selected === 'Funds' ? 'active-nav' : 'inactive-nav' }"
                href="funds" use:link
              >
               <div class="flex justify-between items-center">
                  <div class="text-xl px-2"><Fa icon={faSackDollar} /></div>
                  <div>Funds</div>
                </div>
              </a>
              <a
                class="{ selected === 'Stocks' ? 'active-nav' : 'inactive-nav' }"
                href="/stocks" use:link
              >
               <div class="flex justify-between items-center">
                  <div class="text-xl px-2"><Fa icon={faChartLine} /></div>
                  <div>Stocks</div>
                </div>
              </a>
              <a
                class="{ selected === 'Crypto' ? 'active-nav' : 'inactive-nav' }"
                href="/crypto" use:link
              >
              <div class="flex justify-between items-center">
                  <div class="text-xl px-2"><Fa icon={faBitcoinSign} /></div>
                  <div>Crypto</div>
               </div>
              </a>
          </div>
        </div>
        <a class="mr-1 text-blue-900" href="#" on:click="{() => aboutWndIsVisible = true }">
            <div class="text-xl px-2"><Fa icon={faCircleInfo} /></div>
        </a>
      </div>
  </nav>

<Router>
    <Route path="/">
      <Overview bind:this={activeComponent}/>
    </Route>
    <Route path="funds">
      <Funds bind:this={activeComponent}/>
   </Route>
    <Route path="stocks">
      <Stocks bind:this={activeComponent}/>
   </Route>
    <Route path="crypto">
      <Crypto bind:this={activeComponent}/>
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
