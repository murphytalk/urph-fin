<script lang="ts">
import { onMount, onDestroy } from "svelte";
import { Grid } from "ag-grid-community";
import "ag-grid-community/dist/styles/ag-grid.css";
import "ag-grid-community/dist/styles/ag-theme-alpine.css";

let ref;
let grid: Grid;
let api;

const columnDefs = [
  { field: "make" },
  { field: "model" },
  { field: "price" }
];    

const rowData = [
  { make: "Toyota", model: "Celica", price: 35000 },
  { make: "Toyota", model: "Celica 2", price: 35000 },
  { make: "Ford", model: "Mondeo", price: 32000 },
  { make: "Porsche", model: "Boxster", price: 72000 }
];

// let the grid know which columns and what data to use
const gridOptions = {
  columnDefs: columnDefs,
  rowData: rowData
};

onMount( () => {
    grid = new Grid(ref, gridOptions);    
});

onDestroy(() => {
    if (grid) {
      grid.destroy();
    }
 });

</script>

<div class="w-full h-full flex">
<div bind:this={ref} style="height: 100%; width:100%"  class="ag-theme-alpine"></div>
</div>