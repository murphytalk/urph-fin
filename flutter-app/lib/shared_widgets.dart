import 'package:flutter/material.dart';
import 'package:urph_fin/utils.dart';

Widget financeValueText(String ccy, double value,
    {Color positiveValueColor = Colors.black, Color negativeValueColor = Colors.red}) {
  return Text(formatCcy(ccy, value),
      textAlign: TextAlign.right, style: TextStyle(color: value < 0 ? negativeValueColor : positiveValueColor));
}

class AwaitWidget extends StatelessWidget {
  const AwaitWidget({Key? key, required this.caption}) : super(key: key);
  final String caption;
  @override
  Widget build(BuildContext context) {
    return Column(mainAxisAlignment: MainAxisAlignment.center, children: [
      const SizedBox(
        width: 60,
        height: 60,
        child: CircularProgressIndicator(),
      ),
      DefaultTextStyle(
          style: Theme.of(context).textTheme.headline2!,
          textAlign: TextAlign.center,
          child: Padding(
              padding: const EdgeInsets.only(top: 16),
              child: Directionality(textDirection: TextDirection.ltr, child: Text(caption))))
    ]);
  }
}
