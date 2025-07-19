import 'package:flutter/material.dart';

class FunctionModal extends StatefulWidget {
  final Future<String> Function(
      String functionName, String action, String pid, String eventName) onGo;
  String pid;
  FunctionModal({required this.onGo, required this.pid});

  @override
  _FunctionModalState createState() => _FunctionModalState();
}

class _FunctionModalState extends State<FunctionModal> {
  final TextEditingController _controller = TextEditingController();
  String _selectedAction = 'check for presence';
  bool _isLoading = false;

  Future<void> _handleGo(BuildContext context) async {
    setState(() {
      _isLoading = true;
    });
    String result;
   
    try {
      var eventName = "1";
      
      result = await widget.onGo(_controller.text, _selectedAction, widget.pid,eventName);
    } catch (e) {
      result = e.toString();
    }

    setState(() {
      _isLoading = false;
    });

   
    Navigator.of(context).pop(result);
  }

  @override
  Widget build(BuildContext context) {
    return AlertDialog(
      title: Text('Function Modal'),
      content: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          if (_isLoading)
            Center(child: CircularProgressIndicator())
          else ...[
            TextField(
              controller: _controller,
              decoration: InputDecoration(hintText: 'Enter function name'),
            ),
            SizedBox(height: 16),
            Row(
              children: [
                Expanded(
                  child: DropdownButton<String>(
                    value: _selectedAction,
                    onChanged: (String? newValue) {
                      setState(() {
                        _selectedAction = newValue!;
                      });
                    },
                    items: <String>['check for presence']
                        .map<DropdownMenuItem<String>>((String value) {
                      return DropdownMenuItem<String>(
                        value: value,
                        child: Text(value),
                      );
                    }).toList(),
                  ),
                ),
                SizedBox(width: 16),
                ElevatedButton(
                  onPressed: () => _handleGo(context),
                  child: Text('Go'),
                ),
              ],
            ),
          ],
        ],
      ),
      actions: [
        if (!_isLoading) // Only show the close button when not loading
          TextButton(
            onPressed: () {
              Navigator.of(context).pop();
            },
            child: Text('Close'),
          ),
      ],
    );
  }
}
