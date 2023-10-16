### Overview

**py_objs_stats** is a sample solution to troubleshoot memory accumulating issues due to increasing Python objects. It periodically (every 60 seconds) writes numbers of Python objects per type under /tmp directory, with sequencial number in the file name.


### Prerequisite

* PanoJupyter, to read output file under /tmp.


### How to use

1. Include `py_objs_stats.py` in your application.
1. In your application code, run following code fragment:
    ``` python
    import py_objs_stats
    py_objs_stats.PyObjsStatsThread.instance().start()
    ```
1. Check output files - `/tmp/py_objs_stats.{pid}.{sequence}.txt`.
1. As needed, compare multiple output files to identify which type is increasing.


### Example output

```
<class '_ast.Add'> : 1
<class '_ast.And'> : 1
<class '_ast.AugLoad'> : 1
<class '_ast.AugStore'> : 1
<class '_ast.BitAnd'> : 1
<class '_ast.BitOr'> : 1
<class '_ast.BitXor'> : 1
<class '_ast.Del'> : 1
<class '_ast.Div'> : 1
<class '_ast.Eq'> : 1
    :
<class 'reprlib.Repr'> : 1
<class 'set'> : 68
<class 'staticmethod'> : 17
<class 'threading.Condition'> : 2
<class 'threading.Event'> : 2
<class 'threading._MainThread'> : 1
<class 'tuple'> : 533
<class 'type'> : 207
<class 'types.SimpleNamespace'> : 1
<class 'weakref'> : 604
<class 'wrapper_descriptor'> : 1046
```

### Limitations

* This solution can check which type of Python objects is increasing, but doesn't help if memory is increasing in C/C++, or buffer size of single Python object is expanding (e.g. StringIO).
