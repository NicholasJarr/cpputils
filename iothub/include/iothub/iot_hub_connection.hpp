#ifndef IOT_HUB_CONNECTION
#define IOT_HUB_CONNECTION

#include <cstdlib>
#include <exception>
#include <future>
#include <map>
#include <mutex>
#include <stdexcept>

#include <iothub.h>
#include <iothub_client.h>
#include <iothub_client_options.h>
#include <iothub_device_client.h>
#include <iothubtransportmqtt.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace
{
  const std::chrono::milliseconds IOT_HUB_TIMEOUT(30000);
}

namespace iothub
{
  template <typename T>
  struct Converters
  {
    static T FromJson(const std::string &json_string);
    static std::string ToJson(const T &value);
  };

  template <typename StateType>
  class IoTHubConnection
  {
  public:
    using ConnectionStateCallback = std::function<void(
        IOTHUB_CLIENT_CONNECTION_STATUS, IOTHUB_CLIENT_CONNECTION_STATUS_REASON)>;
    using StateCallback = std::function<void(StateType)>;
    using ErrorCallback = std::function<void()>;
    using Properties = std::map<std::string, std::string>;

    class IoTHubConnectionInitException : public std::runtime_error
    {
    public:
      IoTHubConnectionInitException(const std::string &msg)
          : std::runtime_error(msg) {}
    };
    class IoTHubConnectionConnectionException : public std::runtime_error
    {
    public:
      IoTHubConnectionConnectionException(const std::string &msg)
          : std::runtime_error(msg) {}
    };
    class IoTHubConnectionRequestException : public std::runtime_error
    {
    public:
      IoTHubConnectionRequestException(const std::string &msg)
          : std::runtime_error(msg) {}
    };

    IoTHubConnection(const std::string &connection_string)
    {
      if (IoTHub_Init() != 0)
      {
        throw IoTHubConnectionInitException("Couldn't connect to IoTHub");
      }

      client_handle_ = IoTHubDeviceClient_CreateFromConnectionString(
          connection_string.c_str(), MQTT_Protocol);

      if (client_handle_ == nullptr)
      {
        IoTHub_Deinit();
        throw IoTHubConnectionConnectionException("Couldn't connect to IoTHub");
      }

      // bool traceOn = true;
      // if (IoTHubDeviceClient_SetOption(client_handle_, OPTION_LOG_TRACE,
      //                                  &traceOn) != IOTHUB_CLIENT_OK) {
      //   IoTHubDeviceClient_Destroy(client_handle_);
      //   IoTHub_Deinit();
      //   throw IoTHubConnectionConnectionException("Could't set log tracing.");
      // }

      // std::size_t eventTimeout = 30;
      // if (IoTHubDeviceClient_SetOption(client_handle_,
      //                                  OPTION_EVENT_SEND_TIMEOUT_SECS,
      //                                  &eventTimeout) != IOTHUB_CLIENT_OK) {
      //   IoTHubDeviceClient_Destroy(client_handle_);
      //   IoTHub_Deinit();
      //   throw IoTHubConnectionConnectionException("Could't set AMQP Timeout.");
      // }

      std::size_t timeout = 0;
      if (IoTHubClient_SetRetryPolicy(
              client_handle_, IOTHUB_CLIENT_RETRY_EXPONENTIAL_BACKOFF_WITH_JITTER,
              timeout) != IOTHUB_CLIENT_OK)
      {
        IoTHubDeviceClient_Destroy(client_handle_);
        IoTHub_Deinit();
        throw IoTHubConnectionConnectionException("Could't set retry policy");
      }

      if (IoTHubDeviceClient_SetConnectionStatusCallback(
              client_handle_, ConnectionStatusCallback, this) !=
          IOTHUB_CLIENT_OK)
      {
        IoTHubDeviceClient_Destroy(client_handle_);
        IoTHub_Deinit();
        throw IoTHubConnectionConnectionException(
            "Could't set connection status callback.");
      }

      if (IoTHubDeviceClient_SetDeviceTwinCallback(client_handle_, TwinCallback,
                                                   this) != IOTHUB_CLIENT_OK)
      {
        IoTHubDeviceClient_Destroy(client_handle_);
        IoTHub_Deinit();
        throw IoTHubConnectionConnectionException("Could't set twin callback.");
      }
    }

    virtual ~IoTHubConnection()
    {
      IoTHubDeviceClient_Destroy(client_handle_);
      IoTHub_Deinit();
    }

    void OnConnectionStateChange(ConnectionStateCallback callback)
    {
      std::lock_guard<std::mutex> lock_guard(connection_state_callback_mutex_);
      connection_state_callback_ = callback;
    }

    void OnStateChange(StateCallback callback)
    {
      std::lock_guard<std::mutex> lock_guard(state_callback_mutex_);
      state_callback_ = callback;
    }

    void OnError(ErrorCallback callback)
    {
      std::lock_guard<std::mutex> lock_guard(error_callback_mutex_);
      error_callback_ = callback;
    }

    StateType GetState()
    {
      std::promise<StateType> promise;

      auto ok = IoTHubDeviceClient_GetTwinAsync(
          client_handle_,
          [](DEVICE_TWIN_UPDATE_STATE update_state, const unsigned char *payload,
             size_t size, void *user_context_callback) {
            std::promise<StateType> *result =
                reinterpret_cast<std::promise<StateType> *>(user_context_callback);

            json config = json::parse(payload, payload + size)["desired"];
            result->set_value(Converters<StateType>::FromJson(config.dump()));
          },
          &promise);

      if (ok != IOTHUB_CLIENT_OK)
      {
        IoTHubConnectionRequestException ex("Could't send request to IoTHub");
        promise.set_exception(make_exception_ptr(ex));
      }

      StateType state = GetWithTimeout(promise);
      SetState(state);

      return state;
    }

    void SendReportState(const StateType &state)
    {
      std::string json_str = Converters<StateType>::ToJson(state);

      const unsigned char *payload =
          reinterpret_cast<const unsigned char *>(json_str.c_str());
      auto ok = IoTHubDeviceClient_SendReportedState(
          client_handle_, payload, json_str.size(), SendReportStateCallback,
          this);

      if (ok != IOTHUB_CLIENT_OK)
      {
        throw IoTHubConnectionRequestException("Could't send request to IoTHub");
      }

      SetState(state);
    }

    void SendMessage(const std::string &msg, const Properties &props)
    {
      IOTHUB_MESSAGE_HANDLE message_handle =
          IoTHubMessage_CreateFromString(msg.c_str());

      if (message_handle == nullptr)
      {
        throw IoTHubConnectionRequestException("Could't create message handle");
      }

      if (IoTHubMessage_SetContentTypeSystemProperty(
              message_handle, "application%2fjson") != IOTHUB_MESSAGE_OK)
      {
        throw IoTHubConnectionRequestException(
            "Could't set content type of message");
      }
      if (IoTHubMessage_SetContentEncodingSystemProperty(
              message_handle, "utf-8") != IOTHUB_MESSAGE_OK)
      {
        throw IoTHubConnectionRequestException(
            "Could't set content encoding of message");
      }

      SetProperties(message_handle, props);

      std::size_t tracking_id = tracking_id_count++;
      messages.emplace(tracking_id,
                       SendMessageContext{tracking_id, this, msg, props});
      auto ok = IoTHubDeviceClient_SendEventAsync(client_handle_, message_handle,
                                                  SendMessageCallback,
                                                  &messages[tracking_id]);

      if (ok != IOTHUB_CLIENT_OK)
      {
        throw IoTHubConnectionRequestException("Could't send request to IoTHub");
      }
    }

    void UploadFile(const std::string &file_name, const uint8_t *contents,
                    std::size_t size)
    {
      auto ok = IoTHubClient_UploadToBlobAsync(client_handle_, file_name.c_str(),
                                               contents, size, UploadFileCallback,
                                               this);

      if (ok != IOTHUB_CLIENT_OK)
      {
        throw IoTHubConnectionRequestException("Could't upload file to IoTHub");
      }
    }

  private:
    struct SendMessageContext
    {
      std::size_t tracking_id;
      IoTHubConnection *connection;
      std::string msg;
      Properties props;
    };

    IOTHUB_DEVICE_CLIENT_HANDLE client_handle_;
    StateType state_;
    StateCallback state_callback_;
    ConnectionStateCallback connection_state_callback_;
    ErrorCallback error_callback_;
    std::atomic<std::size_t> tracking_id_count;
    std::map<std::size_t, SendMessageContext> messages;
    std::mutex state_mutex_;
    std::mutex state_callback_mutex_;
    std::mutex connection_state_callback_mutex_;
    std::mutex error_callback_mutex_;

    StateType State()
    {
      std::lock_guard<std::mutex> guard(state_mutex_);
      return state_;
    }

    void SetState(StateType state)
    {
      std::lock_guard<std::mutex> guard(state_mutex_);
      state_ = state;
    }

    void CallStateCallback(StateType state)
    {
      std::lock_guard<std::mutex> guard(state_callback_mutex_);
      state_callback_(state);
    }

    void CallConnectionStateCallback(
        IOTHUB_CLIENT_CONNECTION_STATUS status,
        IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason)
    {
      std::lock_guard<std::mutex> guard(connection_state_callback_mutex_);
      connection_state_callback_(status, reason);
    }

    void CallErrorCallback()
    {
      std::lock_guard<std::mutex> guard(error_callback_mutex_);
      error_callback_();
    }

    void SetProperties(IOTHUB_MESSAGE_HANDLE msg_handle,
                       const Properties &props)
    {
      for (const auto &p : props)
      {
        if (IoTHubMessage_SetProperty(msg_handle, p.first.c_str(),
                                      p.second.c_str()) != IOTHUB_MESSAGE_OK)
        {
          throw std::runtime_error("Bad IoTHub message property");
        }
      }
    }

    template <typename T>
    T GetWithTimeout(std::promise<T> &promise)
    {
      std::future<T> result = promise.get_future();
      std::chrono::milliseconds interval(500), elapsed(0);
      while (result.wait_for(interval) != std::future_status::ready)
      {
        elapsed += interval;
        if (elapsed >= IOT_HUB_TIMEOUT)
        {
          IoTHubConnection::IoTHubConnectionRequestException ex(
              "Could't send request to IoTHub");
          promise.set_exception(make_exception_ptr(ex));
          break;
        }
        std::this_thread::yield();
      }

      return result.get();
    }
    static void SendReportStateCallback(int statusCode,
                                        void *userContextCallback)
    {
      IoTHubConnection *connection =
          reinterpret_cast<IoTHubConnection *>(userContextCallback);

      // Assume status code are like HTTP ones
      if (statusCode < 200 || statusCode >= 400)
      {
        connection->CallErrorCallback();
      }
    }

    static void SendMessageCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result,
                                    void *userContextCallback)
    {
      SendMessageContext *context =
          reinterpret_cast<SendMessageContext *>(userContextCallback);
      IoTHubConnection *connection = context->connection;

      if (result == IOTHUB_CLIENT_CONFIRMATION_MESSAGE_TIMEOUT)
      {
        try
        {
          connection->SendMessage(context->msg, context->props);
          connection->messages.erase(context->tracking_id);
        }
        catch (const std::exception &e)
        {
          connection->CallErrorCallback();
        }
      }
      else if (result == IOTHUB_CLIENT_CONFIRMATION_ERROR)
      {
        connection->CallErrorCallback();
      }
    }

    static void UploadFileCallback(IOTHUB_CLIENT_FILE_UPLOAD_RESULT result,
                                   void *userContextCallback)
    {
      IoTHubConnection *connection =
          reinterpret_cast<IoTHubConnection *>(userContextCallback);

      if (result != FILE_UPLOAD_OK)
      {
        connection->CallErrorCallback();
      }
    }

    static void ConnectionStatusCallback(
        IOTHUB_CLIENT_CONNECTION_STATUS result,
        IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason,
        void *userContextCallback)
    {
      IoTHubConnection *connection =
          reinterpret_cast<IoTHubConnection *>(userContextCallback);

      connection->CallConnectionStateCallback(result, reason);
    }

    static void TwinCallback(DEVICE_TWIN_UPDATE_STATE update_state,
                             const unsigned char *payLoad, size_t size,
                             void *userContextCallback)
    {
      IoTHubConnection *connection =
          reinterpret_cast<IoTHubConnection *>(userContextCallback);

      json body = json::parse(payLoad, payLoad + size);
      if (update_state == DEVICE_TWIN_UPDATE_COMPLETE)
      {
        body = body["desired"];
      }

      json current_state =
          json::parse(Converters<StateType>::ToJson(connection->State()));
      current_state.merge_patch(body);

      StateType result = Converters<StateType>::FromJson(current_state.dump());

      // Check if state changed
      if (result != connection->State())
      {
        connection->CallStateCallback(result);
      }
    }
  };
} // namespace iothub

#endif // !IOT_HUB_CONNECTION