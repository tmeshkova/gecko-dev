/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLiteJSON.h"
#include "nsHashPropertyBag.h"
#include "nsIProperty.h"
#include "EmbedLiteAppService.h"
#include "nsServiceManagerUtils.h"
#include "jsapi.h"
#include "xpcprivate.h"
#include "XPCQuickStubs.h"
#include "nsJSUtils.h"
#include "nsDOMJSUtils.h"
#include "nsContentUtils.h"
#include "mozilla/dom/BindingUtils.h"

using namespace mozilla;

EmbedLiteJSON::EmbedLiteJSON()
{
}

EmbedLiteJSON::~EmbedLiteJSON()
{
}

NS_IMPL_ISUPPORTS(EmbedLiteJSON, nsIEmbedLiteJSON)

nsresult
CreateObjectStatic(nsIWritablePropertyBag2 * *aObject)
{
  nsRefPtr<nsHashPropertyBag> hpb = new nsHashPropertyBag();
  if (!hpb) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  *aObject = hpb.forget().take();
  return NS_OK;
}


NS_IMETHODIMP
EmbedLiteJSON::CreateObject(nsIWritablePropertyBag2 * *aObject)
{
  return CreateObjectStatic(aObject);
}

static bool
JSONCreator(const char16_t* aBuf, uint32_t aLen, void* aData)
{
  nsAString* result = static_cast<nsAString*>(aData);
  result->Append(static_cast<const char16_t*>(aBuf),
                 static_cast<uint32_t>(aLen));
  return true;
}

nsresult
JSValToVariant(JSContext* cx, jsval& propval, nsIWritableVariant* aVariant)
{
  if (propval.isBoolean()) {
    aVariant->SetAsBool(propval.toBoolean());
  } else if (propval.isInt32()) {
    aVariant->SetAsInt32(propval.toInt32());
  } else if (propval.isDouble()) {
    aVariant->SetAsDouble(propval.toDouble());
  } else if (propval.isString()) {
    JS::Rooted<JS::Value> val(cx, propval);
    JSString* propvalString = JS::ToString(cx, val);
    nsAutoJSString vstr;
    if (!propvalString || !vstr.init(cx, propvalString)) {
      return NS_ERROR_FAILURE;
    }
    aVariant->SetAsAString(vstr);
  } else if (!propval.isPrimitive()) {
    NS_ERROR("Value is not primitive");
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult
ParseObject(JSContext* cx, JSObject* object, nsIWritablePropertyBag2* aBag)
{
  JS::RootedObject modObj(cx, object);
  JS::AutoIdArray props(cx, JS_Enumerate(cx, modObj));
  for (size_t i = 0; !!props && i < props.length(); ++i) {
    jsid propid = props[i];
    JS::Rooted<jsid> id(cx, props[i]);
    JS::RootedValue propname(cx);
    JS::Rooted<JS::Value> propval(cx);

    if (!JS_IdToValue(cx, propid, &propname) ||
        !JS_GetPropertyById(cx, modObj, id, &propval)) {
      NS_ERROR("Failed to get property by ID");
      return NS_ERROR_FAILURE;
    }

    JS::Rooted<JS::Value> val(cx, propname);
    JSString* propnameString = JS::ToString(cx, val);
    nsAutoJSString pstr;
    if (!propnameString || !pstr.init(cx, propnameString)) {
      NS_ERROR("Failed to get property string");
      return NS_ERROR_FAILURE;
    }

    if (propval.isPrimitive()) {
      nsCOMPtr<nsIVariant> value;
      nsContentUtils::XPConnect()->JSValToVariant(cx, propval, getter_AddRefs(value));
      nsCOMPtr<nsIWritablePropertyBag> bagSimple = do_QueryInterface(aBag);
      bagSimple->SetProperty(pstr, value);
    } else {
      JS::RootedObject obj(cx, &propval.toObject());
      if (JS_IsArrayObject(cx, obj)) {
        nsCOMPtr<nsIWritableVariant> childElements = do_CreateInstance("@mozilla.org/variant;1");
        uint32_t tmp;
        if (JS_GetArrayLength(cx, obj, &tmp)) {
          nsTArray<nsCOMPtr<nsIVariant>> childArray;
          for (uint32_t i = 0; i < tmp; i++) {
            JS::Rooted<JS::Value> v(cx);
            if (!JS_GetElement(cx, obj, i, &v))
              continue;
            if (v.isPrimitive()) {
              nsCOMPtr<nsIVariant> value;
              nsContentUtils::XPConnect()->JSValToVariant(cx, v, getter_AddRefs(value));
              childArray.AppendElement(value);
            } else {
              nsCOMPtr<nsIWritableVariant> value = do_CreateInstance("@mozilla.org/variant;1");
              nsCOMPtr<nsIWritablePropertyBag2> contextProps;
              CreateObjectStatic(getter_AddRefs(contextProps));
              JSObject* obj = v.toObjectOrNull();
              ParseObject(cx, obj, contextProps);
              value->SetAsInterface(NS_GET_IID(nsIWritablePropertyBag2), contextProps);
              childArray.AppendElement(value);
            }
          }
          childElements->SetAsArray(nsIDataType::VTYPE_INTERFACE_IS, &NS_GET_IID(nsIVariant), childArray.Length(), childArray.Elements());
          nsCOMPtr<nsIWritablePropertyBag> bagSimple = do_QueryInterface(aBag);
          bagSimple->SetProperty(pstr, childElements);
        }
      } else if (JS_IsTypedArrayObject(obj)) {
        NS_ERROR("Don't know how to parse TypedArrayObject");
      } else {
        nsCOMPtr<nsIWritablePropertyBag2> contextPropst;
        CreateObjectStatic(getter_AddRefs(contextPropst));
        if (NS_SUCCEEDED(ParseObject(cx, obj, contextPropst))) {
          nsCOMPtr<nsIWritableVariant> value = do_CreateInstance("@mozilla.org/variant;1");
          value->SetAsInterface(NS_GET_IID(nsIWritablePropertyBag2), contextPropst);
          nsCOMPtr<nsIWritablePropertyBag> bagSimple = do_QueryInterface(aBag);
          bagSimple->SetProperty(pstr, value);
        }
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteJSON::ParseJSON(nsAString const& aJson, nsIPropertyBag2** aRoot)
{
  MOZ_ASSERT(NS_IsMainThread());
  AutoSafeJSContext cx;
  JS::Rooted<JS::Value> json(cx, JSVAL_NULL);
  if (!JS_ParseJSON(cx,
                    static_cast<const char16_t*>(aJson.BeginReading()),
                    aJson.Length(),
                    &json)) {
    NS_ERROR("Failed to parse json string");
    return NS_ERROR_FAILURE;
  }

  if (json.isPrimitive()) {
    NS_ERROR("We don't handle primitive values");
    return NS_ERROR_FAILURE;
  }

  JSObject* obj = json.toObjectOrNull();
  nsCOMPtr<nsIWritablePropertyBag2> contextProps;
  if (obj) {
    CreateObject(getter_AddRefs(contextProps));
    ParseObject(cx, obj, contextProps);
  }

  *aRoot = contextProps.forget().take();
  return NS_OK;
}

static bool SetPropFromVariant(nsIProperty* aProp, JSContext* aCx, JSObject* aObj)
{
  JS::RootedValue rval(aCx);
  nsString name;
  nsCOMPtr<nsIVariant> aVariant;
  aProp->GetValue(getter_AddRefs(aVariant));
  aProp->GetName(name);

  if (!VariantToJsval(aCx, aVariant, &rval)) {
    NS_ERROR("Failed to convert nsIVariant to jsval");
    return false;
  }

  JS::RootedObject obj(aCx, aObj);
  if (!JS_SetProperty(aCx, obj, NS_ConvertUTF16toUTF8(name).get(), rval)) {
    NS_ERROR("Failed to set js object property");
    return false;
  }
  return true;
}

NS_IMETHODIMP
EmbedLiteJSON::CreateJSON(nsIPropertyBag* aRoot, nsAString& outJson)
{
  AutoSafeJSContext cx;
  JS::Rooted<JSObject*> obj(cx, JS_NewObject(cx, nullptr, JS::NullPtr(), JS::NullPtr()));
  if (!obj) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsISimpleEnumerator> windowEnumerator;
  aRoot->GetEnumerator(getter_AddRefs(windowEnumerator));
  bool more;
  windowEnumerator->HasMoreElements(&more);
  while (more) {
    nsCOMPtr<nsISupports> isupp;
    nsCOMPtr<nsIProperty> prop;
    windowEnumerator->GetNext(getter_AddRefs(isupp));
    prop = do_QueryInterface(isupp);
    if (prop) {
      SetPropFromVariant(prop, cx, obj);
    }
    windowEnumerator->HasMoreElements(&more);
  }
  // Example from SaveProfileTask.cpp#74
  JS::Rooted<JS::Value> vlt(cx, OBJECT_TO_JSVAL(obj));
  NS_ENSURE_TRUE(JS_Stringify(cx, &vlt, JS::NullPtr(), JS::NullHandleValue, JSONCreator, &outJson), NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(!outJson.IsEmpty(), NS_ERROR_FAILURE);

  return NS_OK;
}
