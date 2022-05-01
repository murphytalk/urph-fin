<script lang="ts">
import type { ColDef, ColGroupDef } from "ag-grid-community";

import DataGrid from "../share/DataGrid.svelte";
import { currencySign, formatNumber } from "../utils";

interface OverviewItem {
  broker?: string;
  asset?: string;
  ccy?: string;
  marketValue?: number;
  marketValueBaseCcy: number;
  profit?: number;
  profitBaseCcy: number;
}

// for ag-grid
function currencyFormatter(params: any) {
  // console.log('overview num field param %o', params);
  const overview = params.data as OverviewItem;
  // tslint:disable-next-line: max-line-length
  return params.value == null ? '' : `${params.colDef.headerName === 'JPY' ? '\u00a5' : currencySign(overview.ccy)} ${formatNumber(params.value)}`;
}


let data = [
    { make: "Toyota", model: "Celica", price: 35000 },
    { make: "Ford", model: "Mondeo", price: 32000 },
    { make: "Porsche", model: "Boxter", price: 72000 },
  ];

let columnDefs: (ColDef | ColGroupDef)[] = [
    { headerName: 'Broker', field: 'broker', flex: 1 },
    { headerName: 'Asset', field: 'asset', flex: 1 },
    { headerName: 'Currency', field: 'ccy', flex: 1 },
    {
      headerName: 'Market Value', children: [
        { headerName: 'CCY', field: 'marketValue', valueFormatter: currencyFormatter, type: 'numericColumn', flex: 2 },
        { headerName: 'JPY', field: 'marketValueBaseCcy', valueFormatter: currencyFormatter, type: 'numericColumn', flex: 2 }
      ]
    },
    {
      headerName: 'Profit', children: [
        { headerName: 'CCY', field: 'profit', valueFormatter: currencyFormatter, type: 'numericColumn', flex: 2 },
        { headerName: 'JPY', field: 'profitBaseCcy', valueFormatter: currencyFormatter, type: 'numericColumn', flex: 2 }
      ]
    }    
  ];

</script>

<DataGrid {columnDefs} bind:data/>