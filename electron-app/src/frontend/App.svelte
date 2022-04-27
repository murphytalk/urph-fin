<script lang="ts">
	export let name: string;
	import Icon from 'svelte-awesome';
	import { infoCircle } from 'svelte-awesome/icons';
</script>

<nav class="
  relative
  w-full
  flex flex-wrap
  items-center
  justify-between
  py-4
  bg-gray-100
  text-gray-500
  hover:text-gray-700
  focus:text-gray-700
  shadow-lg
  navbar navbar-expand-lg navbar-light
  ">
  <div class="container-fluid w-full flex flex-wrap items-center justify-between px-6">
  <button class="
      navbar-toggler
      text-gray-500
      border-0
      hover:shadow-none hover:no-underline
      py-2
      px-2.5
      bg-transparent
      focus:outline-none focus:ring-0 focus:shadow-none focus:no-underline
    " type="button" data-bs-toggle="collapse" data-bs-target="#navbarSupportedContent"
  aria-controls="navbarSupportedContent" aria-expanded="false" aria-label="Toggle navigation">
  <svg aria-hidden="true" focusable="false" data-prefix="fas" data-icon="bars"
    class="w-6" role="img" xmlns="http://www.w3.org/2000/svg" viewBox="0 0 448 512">
    <path fill="currentColor"
      d="M16 132h416c8.837 0 16-7.163 16-16V76c0-8.837-7.163-16-16-16H16C7.163 60 0 67.163 0 76v40c0 8.837 7.163 16 16 16zm0 160h416c8.837 0 16-7.163 16-16v-40c0-8.837-7.163-16-16-16H16c-8.837 0-16 7.163-16 16v40c0 8.837 7.163 16 16 16zm0 160h416c8.837 0 16-7.163 16-16v-40c0-8.837-7.163-16-16-16H16c-8.837 0-16 7.163-16 16v40c0 8.837 7.163 16 16 16z">
    </path>
  </svg>
  </button>
  <div class="collapse navbar-collapse flex-grow items-center" id="navbarSupportedContent">
  <a class="
        flex
        items-center
        text-gray-900
        hover:text-gray-900
        focus:text-gray-900
        mt-2
        lg:mt-0
        mr-1"
        href="dashboard">
    <img src="images/crab.png" style="height: 48px" alt=""
      loading="lazy" />
  </a>
  <!-- Left links -->
  <ul class="navbar-nav flex flex-col pl-0 list-style-none mr-auto">
    <li class="nav-item p-2">
      <a class="nav-link text-gray-500 hover:text-gray-700 focus:text-gray-700 p-0" href="dashboard">Dashboard</a>
    </li>
  </ul>
  <!-- Left links -->
  </div>
  <!-- Collapsible wrapper -->

  <!-- Right elements -->
  <div class="flex items-center relative">
	<!-- Icon -->
	<!-- svelte-ignore a11y-invalid-attribute -->
	<a class="text-gray-500 hover:text-gray-700 focus:text-gray-700 mr-4 cursor-pointer" data-bs-toggle="modal" data-bs-target="#infoModel" href="#">
 	 	<Icon data={infoCircle} label="Information" scale={2}/>
	</a>
  </div>
  <!-- Right elements -->
  </div>
</nav>

<!-- Modal -->
<div class="modal fade fixed top-0 left-0 hidden w-full h-full outline-none overflow-x-hidden overflow-y-auto" id="infoModel" tabindex="-1" aria-labelledby="infoModalLabel" aria-hidden="true">
  <div class="modal-dialog relative w-auto pointer-events-none">
    <div class="modal-content border-none shadow-lg relative flex flex-col w-full pointer-events-auto bg-white bg-clip-padding rounded-md outline-none text-current">
      <div class="modal-header flex flex-shrink-0 items-center justify-between p-4 border-b border-gray-200 rounded-t-md">
        <h5 class="text-xl font-medium leading-normal text-gray-800" id="infoModelLabel">Information</h5>
        <button type="button"
          class="btn-close box-content w-4 h-4 p-1 text-black border-none rounded-none opacity-50 focus:shadow-none focus:outline-none focus:opacity-100 hover:text-black hover:opacity-75 hover:no-underline"
          data-bs-dismiss="modal" aria-label="Close"/>
      </div>
      <div class="modal-body relative p-4">
        Modal body text goes here.
      </div>
      <div
        class="modal-footer flex flex-shrink-0 flex-wrap items-center justify-end p-4 border-t border-gray-200 rounded-b-md">
        <button type="button" class="px-6
                      py-2.5
                      bg-purple-600
                      text-white
                      font-medium
                      text-xs
                      leading-tight
                      uppercase
                      rounded
                      shadow-md
                      hover:bg-purple-700 hover:shadow-lg
                      focus:bg-purple-700 focus:shadow-lg focus:outline-none focus:ring-0
                      active:bg-purple-800 active:shadow-lg
                      transition
                      duration-150
                      ease-in-out" data-bs-dismiss="modal">Close</button>
      </div>
	  </div>
  </div>
</div>


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
