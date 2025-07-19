import 'dart:convert';
import 'dart:io';
import 'package:path_provider/path_provider.dart';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

class CustomAlertDialog extends StatefulWidget {
  final MethodChannel platform;
  final String pid;
  CustomAlertDialog({required this.pid, required this.platform});
  @override
  _CustomAlertDialogState createState() => _CustomAlertDialogState();
}

class _CustomAlertDialogState extends State<CustomAlertDialog> {
  Map<String, dynamic>? jsonData;
  String? selectedFunction;
  List<Map<String, dynamic>> dynamicFields = [];
  bool isLoading = false;
  bool monitor = false;

  Map<String, TextEditingController> controllers = {};

  Future<void> _loadJsonData() async {
    try {
      String jsonString = await rootBundle.loadString('assets/data.json');
      setState(() {
        jsonData = json.decode(jsonString);
      });
    } catch (e) {
      print("Error loading JSON: $e");
    }
  }

  Future<void> _writeHookConfigFile(Map<String, dynamic> data) async {
    try {
      final projectRoot = Directory.current.path; // Flutter project root
      final directoryPath = '$projectRoot/hook_configs';
      final filePath = '$directoryPath/hook_config.json';

      final directory = Directory(directoryPath);
      if (!await directory.exists()) {
        await directory.create(recursive: true);
      }

      final file = File(filePath);
      final jsonString = const JsonEncoder.withIndent('  ').convert(data);
      await file.writeAsString(jsonString);

      print("Hook config file written to: $filePath");
    } catch (e) {
      print("Failed to write config file: $e");
    }
  }

  void _onFunctionSelected(String? functionName) {
    if (functionName != null && jsonData != null) {
      setState(() {
        selectedFunction = functionName;
        isLoading = true;
        dynamicFields.clear();
        controllers.clear();
      });

      final functionData = jsonData![functionName];
      final fields = <Map<String, dynamic>>[];

      if (functionData.containsKey('match_path')) {
        fields.add({
          "label": "match_path",
          "value": functionData['match_path'].toString(),
          "isBool": false
        });
      }

      if (functionData.containsKey('block')) {
        fields.add({
          "label": "block",
          "value": functionData['block'].toString(),
          "isBool": true
        });
      }

      if (functionData.containsKey('override')) {
        final override = functionData['override'];
        override.forEach((key, value) {
          fields.add({
            "label": key,
            "value": value.toString(),
            "isBool": value is bool ||
                value.toString().toLowerCase() == 'true' ||
                value.toString().toLowerCase() == 'false'
          });
        });
      }

      setState(() {
        dynamicFields = fields;

        for (var field in dynamicFields) {
          final label = field['label'];
          controllers[label] = TextEditingController(text: field['value']);
        }

        isLoading = false;
      });
    }
  }

  void _showUnhookDialog() {
    showDialog(
      context: context,
      barrierDismissible: false, // Prevent dismissal
      builder: (BuildContext context) {
        return AlertDialog(
          title: const Text('Function successfully hooked'),
          content: const Text("Dont forget to unhook it!"),
          actions: [
            TextButton(
              onPressed: () async {
                try {
                  await widget.platform.invokeMethod('unHook',widget.pid);
                  Navigator.of(context).pop(); // Close the dialog
                } catch (e) {
                  print("Error calling unHook: $e");
                }
              },
              child: Text('Unhook'),
            ),
          ],
        );
      },
    );
  }

  Future<void> _sendDataToMethodChannel() async {
    if (selectedFunction == null) return;

    final Map<String, dynamic> overrideSection = {};

    for (var field in dynamicFields) {
      final label = field['label'];
      print(label);
      final rawValue = controllers[label]?.text ?? '';
      dynamic value;
      print(value);
      if (field['isBool'] == true) {
        value = rawValue.toLowerCase() == 'true';
      } else if (field["default"] == true) {
        value = "default";
      } else if (int.tryParse(rawValue) != null) {
        value = int.parse(rawValue);
      } else {
        value = rawValue.isNotEmpty ? rawValue : "default";
      }

      overrideSection[label] = value;
    }

    final Map<String, dynamic> resultData = {
      selectedFunction!: {"override": overrideSection},
      "monitor": monitor.toString(),
      "pid": widget.pid
    };

    try {
      print(resultData);
      await _writeHookConfigFile(resultData);
      final result = await widget.platform
          .invokeMethod<dynamic>('hookFunction', resultData);
      String resultBool = result as String;
      if (resultBool=="success") {
        _showUnhookDialog();
      }

      print("Data sent to method channel: $resultData");
    } catch (e) {
      print("Error sending data to MethodChannel: $e");
    }
  }

  @override
  void initState() {
    super.initState();
    _loadJsonData();
  }

  @override
  void dispose() {
    for (var controller in controllers.values) {
      controller.dispose();
    }
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return AlertDialog(
      title: Row(
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        children: [
          Text('Select Function to Hook'),
          IconButton(
            icon: Icon(Icons.close),
            onPressed: () => Navigator.pop(context),
          ),
        ],
      ),
      content: jsonData == null
          ? Center(child: CircularProgressIndicator())
          : Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                DropdownButton<String>(
                  value: selectedFunction,
                  hint: Text('Select a function'),
                  onChanged: _onFunctionSelected,
                  items: jsonData!.keys.map((functionName) {
                    return DropdownMenuItem(
                      value: functionName,
                      child: Text(functionName),
                    );
                  }).toList(),
                ),
                if (isLoading)
                  const Padding(
                    padding: EdgeInsets.all(8.0),
                    child: CircularProgressIndicator(),
                  )
                else
                  Column(
                    children: dynamicFields.map((field) {
                      final label = field['label'];
                      final isBool = field['isBool'] ? true : false;

                      return Padding(
                        padding: const EdgeInsets.symmetric(vertical: 8.0),
                        child: Row(
                          children: [
                            Expanded(
                              child: isBool
                                  ? Row(
                                      mainAxisAlignment:
                                          MainAxisAlignment.spaceBetween,
                                      children: [
                                        Text(label),
                                        Switch(
                                          value: controllers[label]!.text ==
                                              'true',
                                          onChanged: (bool value) {
                                            setState(() {
                                              controllers[label]!.text =
                                                  value.toString();
                                            });
                                          },
                                        ),
                                      ],
                                    )
                                  : TextField(
                                      controller: controllers[label],
                                      enabled: !(field['default'] ?? false),
                                      decoration: InputDecoration(
                                        labelText: label,
                                      ),
                                    ),
                            ),
                            if (!isBool)
                              Tooltip(
                                message: 'Default',
                                child: Checkbox(
                                  value: field['default'] ?? false,
                                  onChanged: (bool? value) {
                                    setState(() {
                                      field['default'] = value ?? false;
                                    });
                                  },
                                ),
                              ),
                          ],
                        ),
                      );
                    }).toList(),
                  ),
              ],
            ),
      actions: dynamicFields.isNotEmpty && !isLoading
          ? [
              Row(
                children: [
                  TextButton(
                    onPressed: () async{
                      await _sendDataToMethodChannel();
                    },
                    child: Text('Hook'),
                  ),
                ],
              ),
            ]
          : null,
    );
  }
}
