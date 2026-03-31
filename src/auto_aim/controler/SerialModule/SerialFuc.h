#ifndef SERIALFUC_H
#define SERIALFUC_H

/*************************************************/
//
// ����ͨѶ�ؽڳ�Ա
//
/*************************************************/
#define Linux
#include <cmath>
#include <chrono>
#include <string>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <iostream>
#include <vector>
#include <cstring>
#include <cstdint>

namespace rm
{
	// ��һ��float����60Ϊ��,ת��Ϊ���������ֺ�С������
	static void split_60(uint8_t& integer, uint8_t& decimal, float input)
	{
		input = -input; //... ������
		if (input <= 0) {
			integer = -ceil(input);
			decimal = floor(abs(input - ceil(input)) * 100);
		}
		else {
			integer = floor(input) + 60;
			decimal = floor((input - floor(input)) * 100);
		}
	};

	// ��һ��float����30Ϊ��,ת��Ϊ���������ֺ�С������
	static void split_30(uint8_t& integer, uint8_t& decimal, float input)
	{
		if (input < 0) {
			integer = -ceil(input) + 30;
			decimal = floor(abs(input - ceil(input)) * 100);
		}
		else {
			integer = floor(input);
			decimal = floor((input - floor(input)) * 100);
		};
	};

	inline float Hex_To_Decimal(unsigned char* Byte)
	{
		return *((float*)Byte);
	};


	/* ���ڻ��� */
	class SerialBase
	{
	public:
		// ������
		// 0: ������
		// 1: ���ȷ�
		// 2: ������ʼ��
		// 3: ����ֹͣ��
		// 4: CRC8У���
		// 5: CRC16У���
		int operator_;
		int size; // ��ռ�ַ���
		bool variable = false; // �Ƿ�Ϊ����,����������ж�ӦλУ��
		virtual std::vector<uint8_t> process() = 0; // ����
		std::vector<uint8_t> _read_set_data_; // �������������
	};

	/* ������ */
	class Constant : public SerialBase
	{
	public:
		Constant(const uint8_t& data) :data(data)
		{
			operator_ = 0;
			size = 1;
		};
		std::vector<uint8_t> process() override
		{
			return { data };
		};
	private:
		const uint8_t data;
	};

	/* uint_8���� */
	class Uint_8 : public SerialBase
	{
	public:
		Uint_8()
		{
			operator_ = 0;
			size = 1;
			variable = true;
		};
		void set_data(uint8_t data) {
			this->data = data;
		};
		int back_data() {
			return (int)this->_read_set_data_[0];
		};
		std::vector<uint8_t> process() override
		{
			return { data };
		};
	private:
		uint8_t data = 0;
	};

	/* Float30���� */
	class Float30 : public SerialBase
	{
	public:
		Float30()
		{
			operator_ = 0;
			size = 2;
			variable = true;
		};
		void set_data(float input) {
			uint8_t integer, decimal;
			split_30(integer, decimal, input);
			this->data = { integer, decimal };
		};
		std::vector<uint8_t> process() override
		{
			return data;
		};
	private:
		std::vector<uint8_t> data = { 0,0 };
	};

	/* Float60���� */
	class Float60 : public SerialBase
	{
	public:
		Float60()
		{
			operator_ = 0;
			size = 2;
			variable = true;
		};
		void set_data(float input) {
			uint8_t integer, decimal;
			split_60(integer, decimal, input);
			this->data = { integer, decimal };
		};
		std::vector<uint8_t> process() override
		{
			return data;
		};
	private:
		std::vector<uint8_t> data = { 0,0 };
	};

	/* ��Float���� */
	class Float : public SerialBase
	{
	public:
		Float()
		{
			operator_ = 0;
			size = 4;
			variable = true;
		};
		void set_data(float input) {
			uint8_t byte[4];
			std::memcpy(byte, &input, sizeof(input));
			this->data = { byte[0], byte[1] , byte[2] ,byte[3] };
			//const uint8_t* pre__ = reinterpret_cast<const uint8_t*>(&data);
			//this->data = { pre__[0], pre__[1] , pre__[2] ,pre__[3] };
		};
		std::vector<uint8_t> process() override
		{
			return data;
		};
		float back_data() {
			auto byte = _read_set_data_.data();
			float value;
			std::memcpy(&value, byte, sizeof(value));
			return value;
		};

	private:
		std::vector<uint8_t> data = { 0,0,0,0 };
	};

	/* ��int���� */
	class Int : public SerialBase
	{
	public:
		Int()
		{
			operator_ = 0;
			size = 2;
			variable = true;
		};
		void set_data(int input) {
			// Protocol uses 2-byte integer payload.
			int16_t value = static_cast<int16_t>(input);
			uint8_t byte[2];
			std::memcpy(byte, &value, sizeof(byte));
			this->data = { byte[0], byte[1] };
			//const uint8_t* pre__ = reinterpret_cast<const uint8_t*>(&data);
			//this->data = { pre__[0], pre__[1] , pre__[2] ,pre__[3] };
		};
		std::vector<uint8_t> process() override
		{
			return data;
		};
		int back_data() {
			auto byte = _read_set_data_.data();
			int16_t value = 0;
			std::memcpy(&value, byte, sizeof(value));
			return static_cast<int>(value);
		};

	private:
		std::vector<uint8_t> data = { 0,0 };
	};

	/* ���ȷ����� */
	class Length : public SerialBase
	{
	public:
		Length()
		{
			operator_ = 1;
			size = 1;
		};
		std::vector<uint8_t> process() override
		{
			return { 0 };
		};
	};

	/* ������ʼ�� */
	class LengthStart : public Constant
	{
	public:
		LengthStart(const uint8_t& data)
			:Constant(data)
		{
			operator_ = 2;
			size = 1;
		};
	};

	/* ����ֹͣ�� */
	class LengthEnd : public Constant
	{
	public:
		LengthEnd(const uint8_t& data)
			:Constant(data)
		{
			operator_ = 3;
			size = 1;
		};
	};

	/* CRC8У��� */
	class CRC8 : public SerialBase
	{
	public:
		CRC8()
		{
			operator_ = 4;
			size = 1;
		};
		std::vector<uint8_t> process() override
		{
			return { 0 };
		};
	};

	/* CRC16У��� */
	class CRC16 : public SerialBase
	{
	public:
		CRC16()
		{
			operator_ = 5;
			size = 2;
		};
		std::vector<uint8_t> process() override
		{
			return { 0,0 };
		};
	};
};


#endif // !SERIALFUC_H
