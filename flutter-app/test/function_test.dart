import 'dart:ffi';
import 'dart:math';
import 'package:ffi/ffi.dart';
import 'package:flutter/material.dart';
import 'package:test/test.dart';
import 'package:urph_fin/overview.dart';
import 'package:urph_fin/utils.dart';

void main() {
  group('Overview', () {
    // prepare test data
    /*
      Lvl1  Lvl2  Lvl3  value  value(JPY)    profit    profit(JPY)
      ------------------------------------------------------------
      Cash                     10000                        0
              B1                4000                        0        
                   USD    200   2000              0         0 
                   JPY   2000   2000              0         0 
              B2                6000              0         0
                   JPY   6000   6000              0         0 

      Fund                     10000                     5000
              B1                3000                     2000  
                   JPY   3000   3000           2000      2000  
              B2                7000                     3000  
                   JPY   7000   7000           3000      3000  

      Stock                     2000                    -1000
              B3                2000                    -1000
                   USD    100   1000            100      1000
                   JPY   1000   1000          -2000     -2000     
    */

    const value = 22000.0;
    const profit = 4000.0;

    const headerTxtStyle = TextStyle();

    const cashPtr = 'Cash';
    const fundPtr = 'Fund';
    const stockPtr = 'Stock';
    const b1Ptr = 'B1';
    const b2Ptr = 'B2';
    const b3Ptr = 'B3';
    const usdPtr = 'USD';
    const jpyPtr = 'JPY';

    late TableItems items;

    setUpAll(() {
      items = TableItems.withItems([
        // Cash
        TableItem(Level.lvl1, cashPtr, 10000.0, 0.0),
        TableItem(Level.lvl2, b1Ptr, 4000.0, 0.0),
        TableItem.leaf(usdPtr, usdPtr, 200.0, 2000.0, 0.0, 0.0),
        TableItem.leaf(jpyPtr, jpyPtr, 2000.0, 2000.0, 0.0, 0.0),
        TableItem(Level.lvl2, b2Ptr, 6000.0, 0.0),
        TableItem.leaf(jpyPtr, jpyPtr, 6000.0, 6000.0, 0.0, 0.0),
        // Fund
        TableItem(Level.lvl1, fundPtr, 10000.0, 5000.0),
        TableItem(Level.lvl2, b1Ptr, 3000.0, 2000.0),
        TableItem.leaf(jpyPtr, jpyPtr, 3000.0, 3000.0, 2000.0, 2000.0),
        TableItem(Level.lvl2, b2Ptr, 7000.0, 3000.0),
        TableItem.leaf(jpyPtr, jpyPtr, 7000.0, 7000.0, 3000.0, 3000.0),
        // Stock
        TableItem(Level.lvl1, stockPtr, 2000.0, -1000.0),
        TableItem(Level.lvl2, b3Ptr, 2000.0, -1000.0),
        TableItem.leaf(usdPtr, usdPtr, 100.0, 1000.0, 100.0, 1000.0),
        TableItem.leaf(jpyPtr, jpyPtr, 1000.0, 1000.0, -2000.0, -2000.0)
      ], value, profit);
    });

    Widget getColumn(TableRow row, int colIdx) {
      final a = row.children?[colIdx];
      expect(a == null, false);
      return a!;
    }

    Text getColumnTextWidget(TableRow row, int colIdx) {
      final w = getColumn(row, colIdx);
      Text? t;
      if (w is Text) {
        t = w;
      } else if (w is TableCell) {
        expect(w.child is Row, true);
        final r = w.child as Row;
        expect(r.children.length, 2);
        expect(r.children[1] is Text, true);
        t = r.children[1] as Text;
      }
      expect(t == null, false);
      return t!;
    }

    void verifyHeaderRow(TableRow headerRow) {
      // value
      final c = getColumnTextWidget(headerRow, 4);
      expect(c.data, '¥22,000');
      // profit
      final p = getColumnTextWidget(headerRow, 6);
      expect(p.data, '¥4,000');
    }

    void verifyCashGroupRow(TableRow cashRow) {
      final lvl1 = getColumnTextWidget(cashRow, 0);
      expect(lvl1.data, 'Cash');
      final lvl2 = getColumnTextWidget(cashRow, 1);
      expect(lvl2.data, '');
      final lvl3 = getColumnTextWidget(cashRow, 2);
      expect(lvl3.data, '');
      final value = getColumnTextWidget(cashRow, 3);
      expect(value.data, '');
      final valueMainCcy = getColumnTextWidget(cashRow, 4);
      expect(valueMainCcy.data, '¥10,000');
      final profit = getColumnTextWidget(cashRow, 5);
      expect(profit.data, '');
      final profitMainCcy = getColumnTextWidget(cashRow, 6);
      expect(profitMainCcy.data, '');
    }

    void verifyFundGroupRow(TableRow fundRow) {
      final lvl1 = getColumnTextWidget(fundRow, 0);
      expect(lvl1.data, 'Fund');
      final lvl2 = getColumnTextWidget(fundRow, 1);
      expect(lvl2.data, '');
      final lvl3 = getColumnTextWidget(fundRow, 2);
      expect(lvl3.data, '');
      final value = getColumnTextWidget(fundRow, 3);
      expect(value.data, '');
      final valueMainCcy = getColumnTextWidget(fundRow, 4);
      expect(valueMainCcy.data, '¥10,000');
      final profit = getColumnTextWidget(fundRow, 5);
      expect(profit.data, '');
      final profitMainCcy = getColumnTextWidget(fundRow, 6);
      expect(profitMainCcy.data, '¥5,000');
    }

    void verifyStockGroupRow(TableRow stockRow) {
      final lvl1 = getColumnTextWidget(stockRow, 0);
      expect(lvl1.data, 'Stock');
      final lvl2 = getColumnTextWidget(stockRow, 1);
      expect(lvl2.data, '');
      final lvl3 = getColumnTextWidget(stockRow, 2);
      expect(lvl3.data, '');
      final value = getColumnTextWidget(stockRow, 3);
      expect(value.data, '');
      final valueMainCcy = getColumnTextWidget(stockRow, 4);
      expect(valueMainCcy.data, '¥2,000');
      final profit = getColumnTextWidget(stockRow, 5);
      expect(profit.data, '');
      final profitMainCcy = getColumnTextWidget(stockRow, 6);
      expect(profitMainCcy.data, '-¥1,000');
      expect(profitMainCcy.style, const TextStyle(color: Colors.red));
    }

    test('only shows lvl1', () {
      final rows = items.populateTableRows(groupByAsset, groupByBroker, groupByCcy);
      expect(rows.length, 4);

      verifyHeaderRow(rows[0]);
      verifyCashGroupRow(rows[1]);
      verifyFundGroupRow(rows[2]);
      verifyStockGroupRow(rows[3]);
    });
  });
}
