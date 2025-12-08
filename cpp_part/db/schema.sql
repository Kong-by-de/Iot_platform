\restrict UkmUiAloRadqgheM8yQ1KQ9Mf7BChC26JLjdANRIhymdedzl7rxHGlDOeTrDKNo

-- Dumped from database version 17.7 (Ubuntu 17.7-0ubuntu0.25.10.1)
-- Dumped by pg_dump version 17.7 (Ubuntu 17.7-0ubuntu0.25.10.1)

SET statement_timeout = 0;
SET lock_timeout = 0;
SET idle_in_transaction_session_timeout = 0;
SET transaction_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = on;
SELECT pg_catalog.set_config('search_path', '', false);
SET check_function_bodies = false;
SET xmloption = content;
SET client_min_messages = warning;
SET row_security = off;

SET default_tablespace = '';

SET default_table_access_method = heap;

--
-- Name: iot_test; Type: TABLE; Schema: public; Owner: -
--

CREATE TABLE public.iot_test (
    id integer NOT NULL,
    device_id text NOT NULL,
    temperature double precision NOT NULL,
    humidity double precision NOT NULL,
    "timestamp" timestamp without time zone DEFAULT CURRENT_TIMESTAMP
);


--
-- Name: iot_test_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

CREATE SEQUENCE public.iot_test_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: iot_test_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: -
--

ALTER SEQUENCE public.iot_test_id_seq OWNED BY public.iot_test.id;


--
-- Name: schema_migrations; Type: TABLE; Schema: public; Owner: -
--

CREATE TABLE public.schema_migrations (
    version character varying NOT NULL
);


--
-- Name: telemetry_data; Type: TABLE; Schema: public; Owner: -
--

CREATE TABLE public.telemetry_data (
    id integer NOT NULL,
    device_id text NOT NULL,
    temperature real NOT NULL,
    humidity real NOT NULL,
    "timestamp" timestamp without time zone DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT valid_humidity CHECK (((humidity >= (0)::double precision) AND (humidity <= (100)::double precision))),
    CONSTRAINT valid_temperature CHECK (((temperature >= ('-50'::integer)::double precision) AND (temperature <= (100)::double precision)))
);


--
-- Name: telemetry_data_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

CREATE SEQUENCE public.telemetry_data_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: telemetry_data_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: -
--

ALTER SEQUENCE public.telemetry_data_id_seq OWNED BY public.telemetry_data.id;


--
-- Name: user_alert; Type: TABLE; Schema: public; Owner: -
--

CREATE TABLE public.user_alert (
    chat_id bigint NOT NULL,
    temp_threshold real,
    hum_threshold real,
    temp_threshold_low real,
    hum_threshold_low real
);


--
-- Name: user_alerts; Type: TABLE; Schema: public; Owner: -
--

CREATE TABLE public.user_alerts (
    chat_id bigint NOT NULL,
    temp_high_threshold real DEFAULT 0,
    temp_low_threshold real DEFAULT 0,
    hum_high_threshold real DEFAULT 0,
    hum_low_threshold real DEFAULT 0,
    created_at timestamp without time zone DEFAULT CURRENT_TIMESTAMP,
    updated_at timestamp without time zone DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT valid_hum_high CHECK (((hum_high_threshold >= (0)::double precision) AND (hum_high_threshold <= (100)::double precision))),
    CONSTRAINT valid_hum_low CHECK (((hum_low_threshold >= (0)::double precision) AND (hum_low_threshold <= (100)::double precision))),
    CONSTRAINT valid_temp_high CHECK (((temp_high_threshold >= (0)::double precision) AND (temp_high_threshold <= (100)::double precision))),
    CONSTRAINT valid_temp_low CHECK (((temp_low_threshold >= (0)::double precision) AND (temp_low_threshold <= (100)::double precision)))
);


--
-- Name: user_device_alerts; Type: TABLE; Schema: public; Owner: -
--

CREATE TABLE public.user_device_alerts (
    id integer NOT NULL,
    chat_id bigint NOT NULL,
    device_id character varying(50) NOT NULL,
    temp_threshold double precision DEFAULT 0,
    temp_threshold_low double precision DEFAULT 0,
    hum_threshold double precision DEFAULT 0,
    hum_threshold_low double precision DEFAULT 0,
    created_at timestamp without time zone DEFAULT CURRENT_TIMESTAMP
);


--
-- Name: user_device_alerts_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

CREATE SEQUENCE public.user_device_alerts_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: user_device_alerts_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: -
--

ALTER SEQUENCE public.user_device_alerts_id_seq OWNED BY public.user_device_alerts.id;


--
-- Name: user_devices; Type: TABLE; Schema: public; Owner: -
--

CREATE TABLE public.user_devices (
    id integer NOT NULL,
    chat_id bigint NOT NULL,
    device_id character varying(50) NOT NULL,
    device_name character varying(100),
    created_at timestamp without time zone DEFAULT CURRENT_TIMESTAMP
);


--
-- Name: user_devices_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

CREATE SEQUENCE public.user_devices_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: user_devices_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: -
--

ALTER SEQUENCE public.user_devices_id_seq OWNED BY public.user_devices.id;


--
-- Name: iot_test id; Type: DEFAULT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.iot_test ALTER COLUMN id SET DEFAULT nextval('public.iot_test_id_seq'::regclass);


--
-- Name: telemetry_data id; Type: DEFAULT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.telemetry_data ALTER COLUMN id SET DEFAULT nextval('public.telemetry_data_id_seq'::regclass);


--
-- Name: user_device_alerts id; Type: DEFAULT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.user_device_alerts ALTER COLUMN id SET DEFAULT nextval('public.user_device_alerts_id_seq'::regclass);


--
-- Name: user_devices id; Type: DEFAULT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.user_devices ALTER COLUMN id SET DEFAULT nextval('public.user_devices_id_seq'::regclass);


--
-- Name: iot_test iot_test_pkey; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.iot_test
    ADD CONSTRAINT iot_test_pkey PRIMARY KEY (id);


--
-- Name: schema_migrations schema_migrations_pkey; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.schema_migrations
    ADD CONSTRAINT schema_migrations_pkey PRIMARY KEY (version);


--
-- Name: telemetry_data telemetry_data_pkey; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.telemetry_data
    ADD CONSTRAINT telemetry_data_pkey PRIMARY KEY (id);


--
-- Name: user_alert user_alert_pkey; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.user_alert
    ADD CONSTRAINT user_alert_pkey PRIMARY KEY (chat_id);


--
-- Name: user_alerts user_alerts_pkey; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.user_alerts
    ADD CONSTRAINT user_alerts_pkey PRIMARY KEY (chat_id);


--
-- Name: user_device_alerts user_device_alerts_chat_id_device_id_key; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.user_device_alerts
    ADD CONSTRAINT user_device_alerts_chat_id_device_id_key UNIQUE (chat_id, device_id);


--
-- Name: user_device_alerts user_device_alerts_pkey; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.user_device_alerts
    ADD CONSTRAINT user_device_alerts_pkey PRIMARY KEY (id);


--
-- Name: user_devices user_devices_chat_id_device_id_key; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.user_devices
    ADD CONSTRAINT user_devices_chat_id_device_id_key UNIQUE (chat_id, device_id);


--
-- Name: user_devices user_devices_pkey; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.user_devices
    ADD CONSTRAINT user_devices_pkey PRIMARY KEY (id);


--
-- Name: idx_telemetry_device_id; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_telemetry_device_id ON public.telemetry_data USING btree (device_id);


--
-- Name: idx_telemetry_timestamp; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_telemetry_timestamp ON public.telemetry_data USING btree ("timestamp" DESC);


--
-- PostgreSQL database dump complete
--

\unrestrict UkmUiAloRadqgheM8yQ1KQ9Mf7BChC26JLjdANRIhymdedzl7rxHGlDOeTrDKNo


--
-- Dbmate schema migrations
--

INSERT INTO public.schema_migrations (version) VALUES
    ('20251203110925');
