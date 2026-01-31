package com.springboot;

import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;
import org.springframework.scheduling.annotation.EnableScheduling;

@SpringBootApplication
@EnableScheduling
public class TradingBotWebApplication 
{
	public static void main(String[] args) 
	{
		SpringApplication.run(TradingBotWebApplication.class, args);
	}
}
