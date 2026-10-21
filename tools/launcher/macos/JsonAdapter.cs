using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json;
using System.Text.Json.Serialization;
// Adapter for the existing Framework serializer contract. Shared models keep
// dictionaries, object arrays and numeric values rather than JsonElement nodes.
namespace System.Web.Script.Serialization
{
    public sealed class JavaScriptSerializer
    {
        public int MaxJsonLength {get;set;} = 8*1024*1024;
        static readonly JsonSerializerOptions Options = new JsonSerializerOptions { IncludeFields=true, Converters={new Objects()} };
        public string Serialize(object value) { return JsonSerializer.Serialize(value,Options); }
        public T Deserialize<T>(string text) { if(text.Length>MaxJsonLength)throw new System.IO.InvalidDataException("Oversized JSON.");return JsonSerializer.Deserialize<T>(text,Options); }
        public object DeserializeObject(string text) { return Deserialize<object>(text); }
        sealed class Objects : JsonConverter<object>
        {
            public override object Read(ref Utf8JsonReader reader,Type type,JsonSerializerOptions options)
            { using(var doc=JsonDocument.ParseValue(ref reader))return Convert(doc.RootElement); }
            static object Convert(JsonElement e) { switch(e.ValueKind) {
                case JsonValueKind.Object:return e.EnumerateObject().ToDictionary(p=>p.Name,p=>Convert(p.Value));
                case JsonValueKind.Array:return e.EnumerateArray().Select(Convert).ToArray();
                case JsonValueKind.String:return e.GetString();
                case JsonValueKind.Number:if(e.TryGetInt32(out var i))return i;if(e.TryGetInt64(out var l))return l;return e.GetDouble();
                case JsonValueKind.True:return true;case JsonValueKind.False:return false;case JsonValueKind.Null:return null;
                default:throw new JsonException("Unsupported JSON value."); } }
            public override void Write(Utf8JsonWriter writer,object value,JsonSerializerOptions options)
            { JsonSerializer.Serialize(writer,value,value.GetType(),options); }
        }
    }
}
