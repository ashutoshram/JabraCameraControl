#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
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
         getCameras(devices);
      }

      bool getCameras(std::vector<std::string>& devices_) {
         return cqi->getAllJabraDevices(devices_);
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
         } else {
            return Brightness;
         }
      }

      bool containsDeviceName(std::string deviceName){
         return (std::find(devices.begin(), devices.end(), deviceName) != devices.end());
      }

      bool isValidPropertyName(std::string propertyName){
         return (std::find(validPropertyNames.begin(), validPropertyNames.end(), propertyName) != validPropertyNames.end());
      }

      bool getProperty(std::string deviceName, std::string property, Property& propval) {
         if (!containsDeviceName(deviceName)) return false;
         if (!isValidPropertyName(property)) return false;
         std::shared_ptr<CameraDeviceInterface> cdi;

         if (camMap.find(deviceName) == camMap.end()) {
            cdi = std::shared_ptr<CameraDeviceInterface>(cqi->openJabraDevice(deviceName));
            camMap.insert(std::make_pair(deviceName, cdi));
         } else {
            cdi = camMap.at(deviceName);
         }
         return cdi->getProperty(StringToPropertyType(property), propval);
      }

      bool setProperty(std::string deviceName, std::string property, int value) {
         if (!containsDeviceName(deviceName)) return false;
         if (!isValidPropertyName(property)) return false;
         std::shared_ptr<CameraDeviceInterface> cdi;

         if (camMap.find(deviceName) == camMap.end()) {
            // not found
            cdi = std::shared_ptr<CameraDeviceInterface>(cqi->openJabraDevice(deviceName));
            camMap.insert(std::make_pair(deviceName, cdi));
         } else {
            cdi = camMap.at(deviceName);
         }
         return cdi->setProperty(StringToPropertyType(property), value);
         return false;
      }

      bool setStreamParams(std::string deviceName, unsigned int width, unsigned int height, std::string format, unsigned int fps) {
         if (!containsDeviceName(deviceName)) return false;
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
         if (!containsDeviceName(deviceName)) return false;
         std::shared_ptr<CameraStreamInterface> csi;
         if (streamMap.find(deviceName) == streamMap.end()) {
            // not found
            csi = std::shared_ptr<CameraStreamInterface>(new CameraStreamInterface(deviceName, 1280, 720, "YUYV", 30));
            streamMap.insert(std::make_pair(deviceName, csi));
         } else {
            csi = streamMap.at(deviceName);
         }

         if (csi->openStream()) {
            return csi->getFrame(ptrFrame, length);
         }

         return false;
      }

      void freeFrame(std::string deviceName) {
         if (!containsDeviceName(deviceName)) return;
         std::shared_ptr<CameraStreamInterface> csi;
         if (streamMap.find(deviceName) == streamMap.end()) {
            // not found
            return;
         } else {
            csi = streamMap.at(deviceName);
         }
         csi->freeFrame();
      }

   private:
      std::unique_ptr<CameraQueryInterface> cqi;
      std::vector<std::string> devices; 
      std::map<std::string, std::shared_ptr<CameraDeviceInterface> > camMap;
      std::map<std::string, std::shared_ptr<CameraStreamInterface> > streamMap;
      std::vector<std::string> validPropertyNames = {"brightness", "contrast", "saturation", "sharpness", "whitebalance"};
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

static PyObject *PyJabraCamera_getProperty(PyJabraCamera *self, PyObject *args)
{
   const char * property;
   const char * deviceName;
   Property propVal;

   if (!PyArg_ParseTuple(args, "ss", &deviceName, &property)){
      Py_RETURN_NONE;
   }

   bool ret = (self->ptrObj)->getProperty(deviceName, property, propVal);

   if (ret) {
      return Py_BuildValue("iii", propVal.value, propVal.min, propVal.max);
   }
   Py_RETURN_NONE;
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

   if (list.size() > 0) {
      PyObject * tuple = PyTuple_New(list.size());
      for(size_t k=0;k<list.size();k++) {
         PyObject * pValue = PyUnicode_FromString(list[k].c_str());
         PyTuple_SetItem(tuple, k, pValue);
      }
      return tuple;
   }

   Py_RETURN_NONE;
}

static PyObject * PyJabraCamera_setStreamParams(PyJabraCamera *self, PyObject *args, PyObject *keywds)
{
   int width;
   int height;
   const char * format = "YUYV";
   const char * deviceName = "";
   int fps = 30;
   const char *kwlist [] = {
      "deviceName",
      "width",
      "height",
      "format",
      "fps",
      NULL
   };


   if (!PyArg_ParseTupleAndKeywords(args, keywds, "sii|si", const_cast<char **>(kwlist), &deviceName, &width, &height, &format, &fps))
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

   if (!PyArg_ParseTuple(args, "s", &deviceName)) {
      NULL;
   }

   unsigned char * ptrFrame;
   unsigned length;
   bool ret = (self->ptrObj)->getFrame(deviceName, ptrFrame, length);

   if (ret) {
      PyObject * result = PyBytes_FromStringAndSize( (char *)ptrFrame, length);
      (self->ptrObj)->freeFrame(deviceName);
      return result;
   }

   Py_RETURN_NONE;
}



static PyMethodDef PyJabraCamera_methods[] = {
   { "setStreamParams", (PyCFunction)PyJabraCamera_setStreamParams, METH_VARARGS | METH_KEYWORDS, "setStreamParams(width, height, format, fps)"},
   { "getFrame", (PyCFunction)PyJabraCamera_getFrame, METH_VARARGS, "Get a Frame"},
   { "getProperty", (PyCFunction)PyJabraCamera_getProperty,    METH_VARARGS,  "Get property" },
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
