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
            } else if (property == "saturation") { 
                return Saturation;
            } else if (property == "sharpness") { 
                return Sharpness;
            } else if (property == "whitebalance") { 
                return WhiteBalance;
            }
        }

        

        bool setProperty(std::string deviceName, std::string property, int value) {
            if (property == "brightness" || property == "contrast" || property == "saturation" || property == "sharpness" || property == "whitebalance") { 
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

        bool setStreamParams(std::string deviceName, unsigned int width, unsigned int height, std::string format, unsigned int fps) {
            std::shared_ptr<CameraStreamInterface> csi;
            if (streamMap.find(deviceName) == streamMap.end()) {
                // not found
                csi = std::shared_ptr<CameraStreamInterface>(new CameraStreamInterface(deviceName, width, height, format, fps));
                streamMap.insert(std::make_pair(deviceName, csi));
            } else {
                csi = streamMap.at(deviceName);
                csi->updateParams(width, height, format, fps);
            }
            return true;
        }

        bool getFrame(std::string deviceName, unsigned char *& ptrFrame, unsigned& length) {
            std::shared_ptr<CameraStreamInterface> csi;
            if (streamMap.find(deviceName) == streamMap.end()) {
                // not found
                csi = std::shared_ptr<CameraStreamInterface>(new CameraStreamInterface(deviceName, width, height, format, fps));
                streamMap.insert(std::make_pair(deviceName, csi));
            } else {
                csi = streamMap.at(deviceName);
            }

            

            
        }

    private:
        std::unique_ptr<CameraQueryInterface> cqi;
        std::map<std::string, std::shared_ptr<CameraDeviceInterface> > camMap;
        std::map<std::string, std::shared_ptr<CameraStreamInterface> > streamMap;
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

static PyObject PyJabraCamera_setStreamParams(Video_device *self, PyObject *args, PyObject *keywds)
{
    int width;
    int height;
    const char * format;
    const char * deviceName;
    int fps;
    static char *kwlist [] = {
        "deviceName",
        "width",
        "height",
        "format",
        "fps",
        NULL
    };

    if (!PyArg_ParseTupleAndKeywords(args, keywds, "sii|si", kwlist, deviceName, &width, &height, &format, &fps))
    {
        Py_RETURN_FALSE;
    }

    bool ret = (self->ptrObj)->setStreamParams(deviceName, width, height, format, fps);

    if (ret) {
        Py_RETURN_TRUE;
    }

    Py_RETURN_FALSE;
        
}

static PyObject *PyJabraCamera_getFrame(PyJabraCamera *self, PyObject *args)
{
    const char * deviceName;
    int value;

    if (!PyArg_ParseTuple(args, "s", &deviceName)) {
        NULL;
    }

    unsigned char * ptrFrame;
    unsigned length;
    bool ret = (self->ptrObj)->getFrame(deviceName, ptrFrame, length);

    if (ret) {
        return PyBytes_FromStringAndSize(
                ptrFrame, length);
    }

    return NULL;
}



static PyMethodDef PyJabraCamera_methods[] = {
    { "setStreamParams", (PyCFunction)PyJabraCamera_setStreamParams, METH_VARARGS | METH_KEYWORDS, "setStreamParams(width, height, format, fps)"},
    { "getFrame", (PyCFunction)PyJabraCamera_getFrame, METH_VARARGS, "Get a Frame"},
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
