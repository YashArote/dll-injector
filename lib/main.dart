import 'dart:io';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:file_picker/file_picker.dart';
import 'dialog.dart';
import 'unload_dll.dart';
import 'hook_dialog.dart';

void main() {
  runApp(MyApp());
}

class MyApp extends StatefulWidget {
  @override
  _MyAppState createState() => _MyAppState();
}

class _MyAppState extends State<MyApp> {
  static const platform =
      MethodChannel('com.example.flutter_cpp_example/method_channel');
  Map<String, List<String>> _cppData = {};

  @override
  void initState() {
    super.initState();
    _getCppData();
  }

  Future<Map<String, List<String>>> _getCppData() async {
    try {
      final result =
          await platform.invokeMethod<Map<dynamic, dynamic>>('getMap');
      if (result != null) {
        _cppData = result.map(
            (key, value) => MapEntry(key as String, List<String>.from(value)));
        return _cppData;
      }
      throw ErrorWidget.withDetails(message: "Cant retrieve processes");
    } on PlatformException catch (e) {
      throw ErrorWidget.withDetails(message: "Cant retrieve processes");
    }
  }

  /* void openDraggableWindow(BuildContext context) {
    Navigator.push(
      context,
      MaterialPageRoute(
        builder: (context) => DraggableWindow(),
      ),
    );
  }*/

  Future<void> _selectDllFile(
      BuildContext context, String procName, String pid) async {
    FilePickerResult? result = await FilePicker.platform.pickFiles(
      type: FileType.custom,
      allowedExtensions: ['dll'],
    );

    if (result != null) {
      String? filePath = result.files.single.path;
      if (filePath != null) {
        final result =
            await platform.invokeMethod<int>('injectDll', [pid, filePath]);
        String resultText = "Injected successfully";
        if (result == 1) {
          resultText = "An error occured";
        } else if (result == 5) {
          resultText = "Please run the app with administrative previlages";
        }
        showDialog<String>(
          // ignore: use_build_context_synchronously
          context: context,
          builder: (BuildContext context) => AlertDialog(
            title: Text(filePath),
            // ignore: prefer_const_constructors
            content: Text(resultText),
            actions: <Widget>[
              TextButton(
                onPressed: () => Navigator.pop(context, 'OK'),
                child: const Text('OK'),
              ),
            ],
          ),
        );
      }
    }
  }

  Future<String> _onGo(
      String functionName, String action, String pid, String eventName) async {
    final result = await platform.invokeMethod<List<Object?>>(
        "isPresent", [pid, functionName, eventName]);
    print(result);
    String noVal = "";
    String yesVal = "";
    if (result![0] == "1" || result[0] == "error") {
      throw Exception("An error occured");
    }
    if (result[0] == "5") {
      throw Exception("Access denied:Make sure u have enough previlages");
    }
    if (action == "check for presence") {
      noVal = "Function not present";
      yesVal =
          'Function present in :${result!.getRange(1, result.length).join(" ")}';
    }
    if (action == "check for use") {
      noVal = "Unable to perform the operation";
      yesVal = 'yes';
    }

    if (result[0] == "no") return noVal;
    return yesVal;
  }

  Future<void> _showUnloadDllDialog(BuildContext ctx, String pid) async {
    await showDialog<void>(
        context: ctx,
        builder: (BuildContext context) {
          return UnloadDll(platform: platform, pid: pid);
        });
  }

  Future<void> _showHookDialog(BuildContext ctx,String pid) async {
    await showDialog(
        context: ctx,
        builder: (BuildContext context) {
          return CustomAlertDialog(platform: platform,pid: pid,);
        });
  }

  Future<void> _showFunctionModal(BuildContext context, String pid) async {
    var result = await showDialog<String>(
      context: context,
      builder: (BuildContext context) {
        return FunctionModal(
          onGo: _onGo,
          pid: pid,
        );
      },
    );
    String? extra;
    if (result == "yes") {
      result =
          "The hook was injected continue using the target process you will be notified when the function is called";
      extra = "Note:-When you are notified the target process will be ended";
    }

    
    if (result != null) {
     
      showDialog(
        context: context,
        builder: (BuildContext context) {
          return AlertDialog(
            title: const Text('Operation Result'),
            content: RichText(
            
              text: TextSpan(
                style: const TextStyle(
                  fontSize: 18.0, 
                  color: Colors.black, 
                ),
                children: <TextSpan>[
                  TextSpan(
                    text: "$result\n",
                    style: const TextStyle(
                      fontFamily: 'Roboto', 
                    ),
                  ),
                  TextSpan(
                    text: extra,
                    style: const TextStyle(
                      fontWeight: FontWeight.bold,
                      fontFamily:
                          'Roboto', 
                    ),
                  ),
                ],
              ),
            ),
            actions: [
              TextButton(
                onPressed: () {
                  Navigator.of(context).pop();
                },
                child: const Text('Close'),
              ),
            ],
          );
        },
      );
    }
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      debugShowCheckedModeBanner: false,
      home: Scaffold(
        backgroundColor: Colors.blueAccent,
        appBar: PreferredSize(
          preferredSize: const Size.fromHeight(60),
          child: AppBar(
            actions: [
              TextButton(
                style: TextButton.styleFrom(
                  foregroundColor: const Color.fromARGB(255, 212, 208, 206),
                ),
                onPressed: () {
                  setState(() {});
                },
                child: const Text('Reload'),
              ),
            ],
            backgroundColor: Theme.of(context).primaryColor,
            automaticallyImplyLeading: false,
            title: const Align(
              alignment: AlignmentDirectional(-1, -1),
              child: Text(
                'DLL Injector',
                style: TextStyle(
                  fontFamily: 'Outfit',
                  color: Colors.white,
                  fontSize: 28,
                  letterSpacing: 0,
                ),
              ),
            ),
            centerTitle: false,
            elevation: 2,
          ),
        ),
        body: SafeArea(
          top: true,
          child: FutureBuilder<Map<String, List<String>>>(
            future: _getCppData(),
            builder: (context, snapshot) {
              if (snapshot.connectionState == ConnectionState.waiting) {
                return const Center(child: CircularProgressIndicator());
              } else if (snapshot.hasError) {
                return Center(child: Text('Error: ${snapshot.error}'));
              } else if (!snapshot.hasData || snapshot.data!.isEmpty) {
                return const Center(child: Text('No data available'));
              }

              final data = snapshot.data!;

              return LayoutBuilder(
                builder: (context, constraints) {
                  double itemWidth =
                      170;
                  int maxCrossAxisCount =
                      (constraints.maxWidth / itemWidth).floor();

                  return GridView.builder(
                    padding: const EdgeInsets.all(10),
                    gridDelegate: SliverGridDelegateWithFixedCrossAxisCount(
                      crossAxisCount:
                          maxCrossAxisCount > 0 ? maxCrossAxisCount : 1,
                      crossAxisSpacing: 10,
                      mainAxisSpacing: 10,
                      childAspectRatio: (itemWidth / 210), 
                    ),
                    itemCount: data.length,
                    itemBuilder: (context, index) {
                      String processName = data.keys.elementAt(index);
                      List<String> details = data[processName]!;
                      String imagePath = details[1].isEmpty
                          ? "C:\\temp\\Icons-For-Exe\\system7434.png"
                          : details[1];
                      return Card(
                        clipBehavior: Clip.antiAliasWithSaveLayer,
                        color: Colors.white,
                        elevation: 10,
                        shape: RoundedRectangleBorder(
                          borderRadius: BorderRadius.circular(8),
                        ),
                        child: Column(
                          mainAxisSize: MainAxisSize.max,
                          children: [
                            ClipRRect(
                              borderRadius: BorderRadius.circular(8),
                              child: Image.file(
                                File(imagePath),
                                width: double.infinity,
                                height: 100,
                                fit: BoxFit.cover,
                              ),
                            ),
                            PopupMenuButton<String>(
                              icon: Icon(
                                Icons.expand_more_rounded,
                                color: Theme.of(context).primaryColor,
                                size: 24,
                              ),
                              onSelected: (String result) async {
                                switch (result) {
                                  case "Op1":
                                    await _selectDllFile(
                                        context, processName, details[0]);
                                  case "Op2":
                                    _showFunctionModal(context, details[0]);
                                  case "Op3":
                                    _showUnloadDllDialog(context, details[0]);
                                  case "Op4":
                                    _showHookDialog(context,details[0]);
                                }
                              },
                              itemBuilder: (BuildContext context) =>
                                  <PopupMenuEntry<String>>[
                                const PopupMenuItem<String>(
                                  value: 'Op1',
                                  child: Text('Inject DLL'),
                                ),
                                const PopupMenuItem<String>(
                                  value: 'Op2',
                                  child: Text('Utils'),
                                ),
                                const PopupMenuItem<String>(
                                  value: 'Op3',
                                  child: Text('Unload Dll'),
                                ),
                                const PopupMenuItem<String>(
                                  value: 'Op4',
                                  child: Text('Hook'),
                                ),
                              ],
                            ),
                            FittedBox(
                              fit: BoxFit.scaleDown,
                              child: Text(
                                processName,
                                style: const TextStyle(
                                  fontFamily: 'Readex Pro',
                                  fontSize: 15,
                                  letterSpacing: 0,
                                ),
                              ),
                            ),
                            Text(
                              details[0],
                              style: const TextStyle(
                                fontFamily: 'Readex Pro',
                                fontSize: 15,
                                letterSpacing: 0,
                              ),
                            ),
                          ],
                        ),
                      );
                    },
                  );
                },
              );
            },
          ),
        ),
      ),
    );
  }
}
    

        /*home: Scaffold(
        appBar: AppBar(
          title: Text('Flutter C++ Example'),
        ),
        body: _cppData.isEmpty
            ? Center(child: CircularProgressIndicator())
            : ListView.builder(
                itemCount: _cppData.length,
                itemBuilder: (context, index) {
                  String key = _cppData.keys.elementAt(index);
                  return ListTile(
                    title: Text(key),
                    subtitle: Text(_cppData[key]!.join(', ')),
                  );
                },
              ),
      ),*/
        
  
