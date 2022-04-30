<style lang="postcss">
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
  export let shown: boolean = false;
  function hide(): void{
    shown = false;
  }
</script>


<div hidden = {!shown} class="relative top-20 mx-auto p-5 border w-96 shadow-lg rounded-md bg-white z-10">
	<div class="mt-3 text-center">
        <slot></slot>
		<div class="items-center px-4 py-3">
			<button
				id="ok-btn"
				class="px-4 py-2 bg-blue-500 text-white text-base font-medium rounded-md w-full shadow-sm hover:bg-blue-600 focus:outline-none focus:ring-2 focus:ring-blue-300"
        on:click="{hide}"
			>
				OK
			</button>
		</div>
    </div>
</div>

{#if shown}
  <div id='modal-overlay'></div>
{/if}

