// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/loadtimes_extension_bindings.h"

#include <math.h>

// ========== Tapir ADD =========
#include <string>
#include <vector>

#include "base/time/time.h"
#include "content/public/renderer/document_state.h"
#include "net/http/http_response_info.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebPerformance.h"
#include "v8/include/v8.h"

// ========== Tapir ADD =========
#include "content/common/frame_messages.h"
#include "content/public/renderer/render_thread.h"
using content::RenderThread;

using blink::WebDataSource;
using blink::WebLocalFrame;
using blink::WebNavigationType;
using blink::WebPerformance;
using content::DocumentState;


// Values for CSI "tran" property
const int kTransitionLink = 0;
const int kTransitionForwardBack = 6;
const int kTransitionOther = 15;
const int kTransitionReload = 16;

namespace extensions_v8 {

static const char* const kLoadTimesExtensionName = "v8/LoadTimes";

class LoadTimesExtensionWrapper : public v8::Extension {
 public:
  // Creates an extension which adds a new function, chrome.loadTimes()
  // This function returns an object containing the following members:
  // requestTime: The time the request to load the page was received
  // loadTime: The time the renderer started the load process
  // finishDocumentLoadTime: The time the document itself was loaded
  //                         (this is before the onload() method is fired)
  // finishLoadTime: The time all loading is done, after the onload()
  //                 method and all resources
  // navigationType: A string describing what user action initiated the load
  //
  // Note that chrome.loadTimes() is deprecated in favor of performance.timing.
  // Many of the timings reported via chrome.loadTimes() match timings available
  // in performance.timing. Timing data will be removed from chrome.loadTimes()
  // in a future release. No new timings or other information should be exposed
  // via these APIs.
  // =========== Tapir ADD =============
  LoadTimesExtensionWrapper() :
    v8::Extension(kLoadTimesExtensionName,
      "var chrome;"
      "if (!chrome)"
      "  chrome = {};"
      "chrome.loadTimes = function() {"
      "  native function GetLoadTimes();"
      "  return GetLoadTimes();"
      "};"
      "chrome.netLog = function() {"
      "  native function GetNetLog();"
      "  return GetNetLog();"
      "};"
      "chrome.csi = function() {"
      "  native function GetCSI();"
      "  return GetCSI();"
      "}") {}

 //  =========== Source ==============
  // LoadTimesExtensionWrapper() :
  //   v8::Extension(kLoadTimesExtensionName,
  //     "var chrome;"
  //     "if (!chrome)"
  //     "  chrome = {};"
  //     "chrome.loadTimes = function() {"
  //     "  native function GetLoadTimes();"
  //     "  return GetLoadTimes();"
  //     "};"
  //     "chrome.csi = function() {"
  //     "  native function GetCSI();"
  //     "  return GetCSI();"
  //     "}") {}

  v8::Local<v8::FunctionTemplate> GetNativeFunctionTemplate(
      v8::Isolate* isolate,
      v8::Local<v8::String> name) override {
    if (name->Equals(v8::String::NewFromUtf8(isolate, "GetLoadTimes"))) {
      return v8::FunctionTemplate::New(isolate, GetLoadTimes);
    } else if (name->Equals(v8::String::NewFromUtf8(isolate, "GetCSI"))) {
      return v8::FunctionTemplate::New(isolate, GetCSI);
    }
    // =========== Tapir ADD =============
    else if (name->Equals(v8::String::NewFromUtf8(isolate, "GetNetLog"))) {
       return v8::FunctionTemplate::New(isolate, GetNetLog);
    }
    return v8::Local<v8::FunctionTemplate>();
  }

  static const char* GetNavigationType(WebNavigationType nav_type) {
    switch (nav_type) {
      case blink::WebNavigationTypeLinkClicked:
        return "LinkClicked";
      case blink::WebNavigationTypeFormSubmitted:
        return "FormSubmitted";
      case blink::WebNavigationTypeBackForward:
        return "BackForward";
      case blink::WebNavigationTypeReload:
        return "Reload";
      case blink::WebNavigationTypeFormResubmitted:
        return "Resubmitted";
      case blink::WebNavigationTypeOther:
        return "Other";
    }
    return "";
  }

  static int GetCSITransitionType(WebNavigationType nav_type) {
    switch (nav_type) {
      case blink::WebNavigationTypeLinkClicked:
      case blink::WebNavigationTypeFormSubmitted:
      case blink::WebNavigationTypeFormResubmitted:
        return kTransitionLink;
      case blink::WebNavigationTypeBackForward:
        return kTransitionForwardBack;
      case blink::WebNavigationTypeReload:
        return kTransitionReload;
      case blink::WebNavigationTypeOther:
        return kTransitionOther;
    }
    return kTransitionOther;
  }

  static void GetLoadTimes(const v8::FunctionCallbackInfo<v8::Value>& args) {
    args.GetReturnValue().SetNull();
    WebLocalFrame* frame = WebLocalFrame::frameForCurrentContext();
    if (!frame) {
      return;
    }
    WebDataSource* data_source = frame->dataSource();
    if (!data_source) {
      return;
    }
    DocumentState* document_state = DocumentState::FromDataSource(data_source);
    if (!document_state) {
      return;
    }
    WebPerformance web_performance = frame->performance();
    // Though request time now tends to be used to describe the time that the
    // request for the main resource was issued, when chrome.loadTimes() was
    // added, it was used to describe 'The time the request to load the page was
    // received', which is the time now known as navigation start. For backward
    // compatibility, we continue to provide request_time, setting its value to
    // navigation start.
    double request_time = web_performance.navigationStart();
    // Developers often use start_load_time as the time the navigation was
    // started, so we return navigationStart for this value as well. See
    // https://gist.github.com/search?utf8=%E2%9C%93&q=startLoadTime.
    // Note that, historically, start_load_time reported the time that a
    // provisional load was first processed in the render process. For
    // browser-initiated navigations, this is some time after navigation start,
    // which means that developers who used this value as a way to track the
    // start of a navigation were misusing this timestamp and getting the wrong
    // value - they should be using navigationStart intead. Additionally,
    // once plznavigate ships, provisional loads will not be processed by the
    // render process for browser-initiated navigations, so reporting the time a
    // provisional load was processed in the render process will no longer make
    // sense. Thus, we now report the time for navigationStart, which is a value
    // more consistent with what developers currently use start_load_time for.
    double start_load_time = web_performance.navigationStart();
    // TODO(bmcquade): Remove this. 'commit' time is a concept internal to
    // chrome that shouldn't be exposed to the web platform.
    double commit_load_time = web_performance.responseStart();
    double finish_document_load_time =
        web_performance.domContentLoadedEventEnd();
    double finish_load_time = web_performance.loadEventEnd();
    double first_paint_time = web_performance.firstPaint();
    // TODO(bmcquade): remove this. It's misleading to track the first paint
    // after the load event, since many pages perform their meaningful paints
    // long before the load event fires. We report a time of zero for the
    // time being.
    double first_paint_after_load_time = 0.0;
    std::string navigation_type =
        GetNavigationType(data_source->navigationType());
    bool was_fetched_via_spdy = document_state->was_fetched_via_spdy();
    bool was_npn_negotiated = document_state->was_npn_negotiated();
    std::string npn_negotiated_protocol =
        document_state->npn_negotiated_protocol();
    bool was_alternate_protocol_available =
        document_state->was_alternate_protocol_available();
    std::string connection_info = net::HttpResponseInfo::ConnectionInfoToString(
        document_state->connection_info());
    // Important: |frame|, |data_source| and |document_state| should not be
    // referred to below this line, as JS setters below can invalidate these
    // pointers.
    v8::Isolate* isolate = args.GetIsolate();
    v8::Local<v8::Context> ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> load_times = v8::Object::New(isolate);
    if (!load_times
             ->Set(ctx, v8::String::NewFromUtf8(isolate, "requestTime",
                                                v8::NewStringType::kNormal)
                            .ToLocalChecked(),
                   v8::Number::New(isolate, request_time))
             .FromMaybe(false)) {
      return;
    }

    if (!load_times
             ->Set(ctx, v8::String::NewFromUtf8(isolate, "startLoadTime",
                                                v8::NewStringType::kNormal)
                            .ToLocalChecked(),
                   v8::Number::New(isolate, start_load_time))
             .FromMaybe(false)) {
      return;
    }
    if (!load_times
             ->Set(ctx, v8::String::NewFromUtf8(isolate, "commitLoadTime",
                                                v8::NewStringType::kNormal)
                            .ToLocalChecked(),
                   v8::Number::New(isolate, commit_load_time))
             .FromMaybe(false)) {
      return;
    }
    if (!load_times
             ->Set(ctx,
                   v8::String::NewFromUtf8(isolate, "finishDocumentLoadTime",
                                           v8::NewStringType::kNormal)
                       .ToLocalChecked(),
                   v8::Number::New(isolate, finish_document_load_time))
             .FromMaybe(false)) {
      return;
    }
    if (!load_times
             ->Set(ctx, v8::String::NewFromUtf8(isolate, "finishLoadTime",
                                                v8::NewStringType::kNormal)
                            .ToLocalChecked(),
                   v8::Number::New(isolate, finish_load_time))
             .FromMaybe(false)) {
      return;
    }
    if (!load_times
             ->Set(ctx, v8::String::NewFromUtf8(isolate, "firstPaintTime",
                                                v8::NewStringType::kNormal)
                            .ToLocalChecked(),
                   v8::Number::New(isolate, first_paint_time))
             .FromMaybe(false)) {
      return;
    }
    if (!load_times
             ->Set(ctx,
                   v8::String::NewFromUtf8(isolate, "firstPaintAfterLoadTime",
                                           v8::NewStringType::kNormal)
                       .ToLocalChecked(),
                   v8::Number::New(isolate, first_paint_after_load_time))
             .FromMaybe(false)) {
      return;
    }
    if (!load_times
             ->Set(ctx, v8::String::NewFromUtf8(isolate, "navigationType",
                                                v8::NewStringType::kNormal)
                            .ToLocalChecked(),
                   v8::String::NewFromUtf8(isolate, navigation_type.c_str(),
                                           v8::NewStringType::kNormal)
                       .ToLocalChecked())
             .FromMaybe(false)) {
      return;
    }
    if (!load_times
             ->Set(ctx, v8::String::NewFromUtf8(isolate, "wasFetchedViaSpdy",
                                                v8::NewStringType::kNormal)
                            .ToLocalChecked(),
                   v8::Boolean::New(isolate, was_fetched_via_spdy))
             .FromMaybe(false)) {
      return;
    }
    if (!load_times
             ->Set(ctx, v8::String::NewFromUtf8(isolate, "wasNpnNegotiated",
                                                v8::NewStringType::kNormal)
                            .ToLocalChecked(),
                   v8::Boolean::New(isolate, was_npn_negotiated))
             .FromMaybe(false)) {
      return;
    }
    if (!load_times
             ->Set(ctx,
                   v8::String::NewFromUtf8(isolate, "npnNegotiatedProtocol",
                                           v8::NewStringType::kNormal)
                       .ToLocalChecked(),
                   v8::String::NewFromUtf8(isolate,
                                           npn_negotiated_protocol.c_str(),
                                           v8::NewStringType::kNormal)
                       .ToLocalChecked())
             .FromMaybe(false)) {
      return;
    }
    if (!load_times
             ->Set(ctx, v8::String::NewFromUtf8(isolate,
                                                "wasAlternateProtocolAvailable",
                                                v8::NewStringType::kNormal)
                            .ToLocalChecked(),
                   v8::Boolean::New(isolate, was_alternate_protocol_available))
             .FromMaybe(false)) {
      return;
    }
    if (!load_times
             ->Set(ctx, v8::String::NewFromUtf8(isolate, "connectionInfo",
                                                v8::NewStringType::kNormal)
                            .ToLocalChecked(),
                   v8::String::NewFromUtf8(isolate, connection_info.c_str(),
                                           v8::NewStringType::kNormal)
                       .ToLocalChecked())
             .FromMaybe(false)) {
      return;
    }
    args.GetReturnValue().Set(load_times);
  }

  static void GetCSI(const v8::FunctionCallbackInfo<v8::Value>& args) {
    args.GetReturnValue().SetNull();
    WebLocalFrame* frame = WebLocalFrame::frameForCurrentContext();
    if (!frame) {
      return;
    }
    WebDataSource* data_source = frame->dataSource();
    if (!data_source) {
      return;
    }
    DocumentState* document_state = DocumentState::FromDataSource(data_source);
    if (!document_state) {
      return;
    }
    base::Time now = base::Time::Now();
    base::Time start = document_state->request_time().is_null()
                           ? document_state->start_load_time()
                           : document_state->request_time();
    base::Time onload = document_state->finish_document_load_time();
    base::TimeDelta page = now - start;
    int navigation_type = GetCSITransitionType(data_source->navigationType());
    // Important: |frame|, |data_source| and |document_state| should not be
    // referred to below this line, as JS setters below can invalidate these
    // pointers.
    v8::Isolate* isolate = args.GetIsolate();
    v8::Local<v8::Context> ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> csi = v8::Object::New(isolate);
    if (!csi->Set(ctx, v8::String::NewFromUtf8(isolate, "startE",
                                               v8::NewStringType::kNormal)
                           .ToLocalChecked(),
                  v8::Number::New(isolate, floor(start.ToDoubleT() * 1000)))
             .FromMaybe(false)) {
      return;
    }
    if (!csi->Set(ctx, v8::String::NewFromUtf8(isolate, "onloadT",
                                               v8::NewStringType::kNormal)
                           .ToLocalChecked(),
                  v8::Number::New(isolate, floor(onload.ToDoubleT() * 1000)))
             .FromMaybe(false)) {
      return;
    }
    if (!csi->Set(ctx, v8::String::NewFromUtf8(isolate, "pageT",
                                               v8::NewStringType::kNormal)
                           .ToLocalChecked(),
                  v8::Number::New(isolate, page.InMillisecondsF()))
             .FromMaybe(false)) {
      return;
    }
    if (!csi->Set(ctx, v8::String::NewFromUtf8(isolate, "tran",
                                               v8::NewStringType::kNormal)
                           .ToLocalChecked(),
                  v8::Number::New(isolate, navigation_type))
             .FromMaybe(false)) {
      return;
    }
    args.GetReturnValue().Set(csi);
  }

  // ========= Tapir ADD =============
  static void GetNetLog(const v8::FunctionCallbackInfo<v8::Value>& args) {
    args.GetReturnValue().SetNull();

    std::vector<std::string> result;
    RenderThread::Get()->Send(new FrameHostMsg_network_log(&result));

    v8::Isolate* isolate = args.GetIsolate();
    v8::Local<v8::Context> ctx = isolate->GetCurrentContext();
    v8::Local<v8::Array> net_log = v8::Array::New(isolate);

    uint32_t count = result.size();

    for (uint32_t i = 0; i < count; i++) {
      if (!net_log->Set(ctx,  i ,v8::String::NewFromUtf8(isolate, result.at(i).c_str(),
                               v8::NewStringType::kNormal).ToLocalChecked()).FromMaybe(false)) {
        continue;
      }
    }
    args.GetReturnValue().Set(net_log);
  }

};

v8::Extension* LoadTimesExtension::Get() {
  return new LoadTimesExtensionWrapper();
}

}  // namespace extensions_v8
