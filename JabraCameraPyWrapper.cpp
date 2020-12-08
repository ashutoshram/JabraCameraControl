#include <Python.h>
#include <numpy/ndarraytypes.h>

#include <cstdio>
#include <iostream>
#include <memory>
#include <string>
#include <map>

#include "CameraDevice.h"

class JabraDriver {
    public:
        JabraDriver() {
            cqi.reset(new CameraQueryInterface);
        }

        bool getCameras(std::vector<std::string>& devices) {
            return cqi->getAllJabraDevices(devices);
        }

        static PropertyType StringToPropertyType(std::string property) {
            if (property == "brightness") {
                return Brightness;
            } else if (property == "contrast") { 
                return Contrast;
            }
            return Brightness;
        }

        bool setProperty(std::string deviceName, std::string property, int value) {
            if (property == "brightness" || property == "contrast") { // FIXME
                std::shared_ptr<CameraDeviceInterface> cdi;
                if (camMap.find(deviceName) == camMap.end()) {
                    // not found
                    cdi = std::shared_ptr<CameraDeviceInterface>(cqi->openJabraDevice(deviceName));
                    camMap.insert(std::make_pair(deviceName, cdi));
                } else {
                    cdi = camMap.at(deviceName);
                }
                return cdi->setProperty(StringToPropertyType(property), value);
            }
            return false;
        }


    private:
        std::unique_ptr<CameraQueryInterface> cqi;
        std::map<std::string, std::shared_ptr<CameraDeviceInterface> > camMap;
        
};

typedef struct {
    PyObject_HEAD
    JabraDriver * ptrObj;
} PyJabraCamera;

static PyModuleDef jabracameramodule = {
    PyModuleDef_HEAD_INIT,
    "jabracamera",
    "Work with Jabra Cameras",
    -1,
    NULL, NULL, NULL, NULL, NULL
};

static int PyJabraCamera_init(PyJabraCamera *self, PyObject *args, PyObject *kwds)
// initialize PyJabraCamera Object
{
    try {
       self->ptrObj = new JabraDriver(); 
    } catch (std::runtime_error& e) {
       PyErr_Format(PyExc_ValueError, "Cannot instantiate CameraQueryInterface");
       return -1;
    }

    return 0;
}

static void PyJabraCamera_dealloc(PyJabraCamera * self)
// destroy the object
{
    delete self->ptrObj;
    Py_TYPE(self)->tp_free(self);
}

static PyObject *PyJabraCamera_setProperty(PyJabraCamera *self, PyObject *args)
{
    const char * property;
    const char * deviceName;
    int value;

    if (!PyArg_ParseTuple(args, "ssi", &deviceName, &property, &value)) {
        Py_RETURN_FALSE;
    }

    bool ret = (self->ptrObj)->setProperty(deviceName, property, value);

    if (ret) {
        Py_RETURN_TRUE;
    }

    Py_RETURN_FALSE;
}

static PyObject *PyJabraCamera_getCameras(PyJabraCamera *self, PyObject *args)
{
    std::vector<std::string> list;
    (self->ptrObj)->getCameras(list);

    PyObject * tuple = nullptr;
    if (list.size() > 0) {
        tuple = PyTuple_New(list.size());
        for(size_t k=0;k<list.size();k++) {
            PyObject * pValue = PyUnicode_FromString(list[k].c_str());
            PyTuple_SetItem(tuple, k, pValue);
        }
    }
    
    return tuple;
}


static PyMethodDef PyJabraCamera_methods[] = {
    { "setProperty", (PyCFunction)PyJabraCamera_setProperty,    METH_VARARGS,  "Set property" },
    { "getCameras", (PyCFunction)PyJabraCamera_getCameras,    METH_VARARGS,  "Get list of cameras" },
    {NULL}  /* Sentinel */
};

static PyTypeObject PyJabraCameraType = { PyVarObject_HEAD_INIT(NULL, 0)
                                    "jabracamera.JabraCamera"   /* tp_name */
                                };


PyMODINIT_FUNC PyInit_jabracamera(void)
// create the module
{
    PyObject* m;

    PyJabraCameraType.tp_new = PyType_GenericNew;
    PyJabraCameraType.tp_basicsize=sizeof(PyJabraCamera);
    PyJabraCameraType.tp_dealloc=(destructor) PyJabraCamera_dealloc;
    PyJabraCameraType.tp_flags=Py_TPFLAGS_DEFAULT;
    PyJabraCameraType.tp_doc="JabraCamera objects";
    PyJabraCameraType.tp_methods=PyJabraCamera_methods;
    PyJabraCameraType.tp_init=(initproc)PyJabraCamera_init;

    if (PyType_Ready(&PyJabraCameraType) < 0)
        return NULL;

    m = PyModule_Create(&jabracameramodule);
    if (m == NULL)
        return NULL;

    Py_INCREF(&PyJabraCameraType);
    PyModule_AddObject(m, "JabraCamera", (PyObject *)&PyJabraCameraType); // Add JabraCamera object to the module
    return m;
}
