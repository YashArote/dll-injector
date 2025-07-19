import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

class UnloadDll extends StatefulWidget {
  final MethodChannel platform;
  final String pid;
  const UnloadDll({required this.platform, required this.pid});

  @override
  State<StatefulWidget> createState() {
    return UnloadDllState();
  }
}

class UnloadDllState extends State<UnloadDll> {
  late Future<Map<int, String>> _dllsFuture;

  @override
  void initState() {
    super.initState();
    _dllsFuture = _getDlls();
  }

  Future<Map<int, String>> _getDlls() async {
    print("Id is:- " + widget.pid);
    final result = await widget.platform
        .invokeMethod<List<dynamic>>("getDlls", [widget.pid]);
    print(result);
    Map<int, String> res = {};
    for (int i = 0; i < result!.length; i++) {
      res[i] = result[i] as String;
    }
    return res;
  }

  Future<bool> _unLoadItem(String moduleName) async {
    final result = await widget.platform
        .invokeMethod<List<dynamic>>("unloadDll", [moduleName, widget.pid]);
    final resAsString = result![0] as String;
    print(resAsString);
    return (resAsString == "yes") ? true : false;
  }

  @override
  Widget build(BuildContext context) {
    return AlertDialog(
      title: Text("Items List"),
      content: FutureBuilder<Map<int, String>>(
        future: _dllsFuture,
        builder: (context, snapshot) {
          if (snapshot.connectionState == ConnectionState.waiting) {
            return const Center(child: CircularProgressIndicator());
          } else if (snapshot.hasError) {
            return Text("Error: ${snapshot.error}");
          } else if (!snapshot.hasData || snapshot.data!.isEmpty) {
            return Text("No items available.");
          } else {
            Map<int, String> items = snapshot.data!;
            List<int> keys = items.keys.toList();
            Map<int, bool> visibilityMap = {for (var key in keys) key: true};

            return StatefulBuilder(
              builder: (BuildContext context, StateSetter setState) {
                return SizedBox(
                  width: 300,
                  height: 300,
                  child: ListView.builder(
                    shrinkWrap: true,
                    itemCount: keys.length,
                    itemBuilder: (context, index) {
                      int key = keys[index];
                      String item = items[key]!;
                      bool isVisible = visibilityMap[key] ?? true;

                      return AnimatedOpacity(
                        opacity: isVisible ? 1.0 : 0.0,
                        duration: const Duration(milliseconds: 300),
                        child: Visibility(
                          visible: isVisible,
                          child: Card(
                            margin: const EdgeInsets.symmetric(vertical: 8),
                            child: ListTile(
                              title: Text(item),
                              trailing: TextButton(
                                onPressed: () async {
                                  bool success = await _unLoadItem(item);
                                  if (success) {
                                    setState(() {
                                      visibilityMap[key] = false;
                                    });

                                    await Future.delayed(
                                        const Duration(milliseconds: 300));

                                    setState(() {
                                      items.remove(key);
                                      keys.removeAt(index);
                                    });

                                    ScaffoldMessenger.of(context).showSnackBar(
                                      SnackBar(content: Text("$item unloaded")),
                                    );
                                  }
                                },
                                child: const Text("Unload"),
                              ),
                            ),
                          ),
                        ),
                      );
                    },
                  ),
                );
              },
            );
          }
        },
      ),
      actions: [
        TextButton(
          onPressed: () {
            Navigator.of(context).pop();
          },
          child: const Text("Close"),
        ),
      ],
    );
  }
}
